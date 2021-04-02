
#include <regex>
#include <filesystem>

#include <boost/fusion/adapted.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/include/define_struct.hpp>
#include <boost/process.hpp>
#include <boost/algorithm/string.hpp>

#include "restc-cpp/RequestBuilder.h"

#include "pulsar.h"
#include "pulsar_api.h"

using namespace std;
using namespace std::string_literals;
using namespace restc_cpp;

namespace purech {

struct PrcCtx {
    using io_buf_t = boost::asio::streambuf;
    PrcCtx(boost::asio::io_context& ctx, const Config& cfg, const std::string& kubeFile)
        : id{nextId++},
          port(id + cfg.localPort),
          name{std::filesystem::path{kubeFile}.filename()},
          ctx{ctx},
          aps{std::make_unique<boost::process::async_pipe>(ctx)},
          ape{std::make_unique<boost::process::async_pipe>(ctx)} {}

    const uint16_t id;
    const uint16_t port;
    const std::string name;
    boost::asio::io_context& ctx;
    std::unique_ptr<boost::process::async_pipe> aps;
    std::unique_ptr<boost::process::async_pipe> ape;
    std::unique_ptr<boost::process::child> child;
    io_buf_t stdout;
    io_buf_t stderr;
    static uint16_t nextId;
    std::promise<bool> started;
    bool pending = true;
};

uint16_t PrcCtx::nextId = 0;

namespace {
void FetchProcessOutput(boost::process::async_pipe& ap,
                        PrcCtx::io_buf_t& buf, PrcCtx& pctx, bool err) {
    boost::asio::async_read_until(
                ap, buf, '\n',
                [&ap, &buf, &pctx, err](const boost::system::error_code& ec, std::size_t /*size*/) {
        std::istream is(&buf);
        std::string line;
        std::getline(is, line);

        if (err) {
            LOG_ERROR << pctx.name << " proxy said: " << line;
            if (pctx.pending) {
                pctx.pending = false;
                pctx.started.set_value(false);
            }
        } else {
            LOG_DEBUG << pctx.name << " proxy said: " << line;
        }

        if (ec) {
            LOG_ERROR << pctx.name << " IO error: " << ec.message();
            if (pctx.pending) {
                pctx.pending = false;
                pctx.started.set_value(false);
            }
            return;
        }

        if (pctx.pending) {
            pctx.pending = false;
            pctx.started.set_value(line.find("Forwarding from") != string::npos);
        }

        FetchProcessOutput(ap, buf, pctx, err);
    });
}

void SetupPortForwarding(const Engine::Cluster& cluster, PrcCtx& pctx) {
    namespace bp = boost::process;
    std::error_code ec;

    std::ostringstream ports;
    ports << pctx.port << ":8080";

    LOG_DEBUG << "Starting port forwarding on " << ports.str() << " on " << cluster.origin;

    pctx.child = make_unique<boost::process::child>(
                bp::search_path("kubectl"), "--kubeconfig", cluster.origin, "-n", cluster.ns,
                "port-forward", "svc/"s + cluster.svcName, ports.str(), bp::std_in.close(),
                bp::std_out > *pctx.aps, bp::std_err > *pctx.ape, ec);

    if (ec) {
        LOG_ERROR << cluster.origin << ": Failed to launch port-forwarding: " << ec;
        throw std::runtime_error("Failed to launch port-forwarding for: "s + cluster.origin);
    }

    FetchProcessOutput(*pctx.aps, pctx.stdout, pctx, false);
    FetchProcessOutput(*pctx.ape, pctx.stderr, pctx, true);
}

string baseUrl(const Engine::Cluster& c) {
    ostringstream url;
    return c.url + "/admin/v2";
}

string stripPersistent(const string& topic) {
  static const auto persistent = "persistent://"s;

  if (topic.substr(0, persistent.size()) == persistent) {
    return topic.substr(persistent.size());
  }

  return topic;
}

template <typename T>
std::string strings(const T& list) {
    ostringstream out;

    out << "[";
    auto cnt = 0;
    for(const auto& n : list) {
        if (++cnt > 1) {
            out << ' ';
        }
        out << n;
    }

    out << ']';
    return out.str();
}


} // ans

Engine::Engine(const Config &config)
    : config_{config}
{

}

void Engine::run()
{
    prepare();

    LOG_INFO << "Fetching information. This may take a little while...";
    for (auto& [_, c] : clusters_) {
        // Use one async co-routine for each cluster
        client_->Process([cluster=c, this](Context &ctx) {
            processCluster(*cluster, ctx);
        });
    }

    client_->CloseWhenReady();

    // Process the information
    LOG_INFO << "Done fetching information.";

    simpleSummary();
}

void Engine::prepare()
{
    client_ = RestClient::Create();

    for(const auto& c : config_.clusters) {
        auto cluster = make_shared<Engine::Cluster>();

        static const regex urlPattern{R"(^https?://.+)", std::regex_constants::icase};

        if (regex_match(c, urlPattern)) {
            cluster->setUrl(c);
        } else if (cluster->setKubeconfig(c) ; filesystem::is_regular_file(cluster->origin)) {
            auto pfw = make_shared<PrcCtx>(client_->GetIoService(), config_, cluster->origin);
            SetupPortForwarding(*cluster, *pfw);
            cluster->url = "http://127.0.0.1:"s + to_string(pfw->port);
            processes_.emplace_back(move(pfw));
            clusters_.emplace(cluster->name, cluster);
        } else {
            LOG_ERROR << "I don't know what '" << c << "' is. "
            << "I expected a URL or a kubeconfig file!";
            throw runtime_error("Unknown origin");
        }
    }

    // Wait for the port-forwarding to start...
    for (auto& pctx : processes_) {
        if (!pctx->started.get_future().get()) {
            LOG_ERROR << "Failed to start port-forwarding for " << pctx->name;
            throw std::runtime_error("Failed to start port-forwarding for: "s + pctx->name);
        }
    }
}

void Engine::processCluster(Engine::Cluster &cluster, Context &ctx)
{
    unique_ptr<regex> tfilter;
    if (!config_.topicFilter.empty()) {
      tfilter = make_unique<regex>(config_.topicFilter);
    }

    // Figure out the local cluster name
    if (cluster.name.empty()) {
        LOG_ERROR << cluster.origin << " No local cluster-name provided.";
        throw runtime_error("Unable to determine local cluster-name");
    }

    // Get cluster names
    SerializeFromJson(cluster.clusters,
                      RequestBuilder(ctx).Get(baseUrl(cluster) + "/clusters").Execute());

    // Check that our name is there

    // Get tenants
    vector<string> tenants;
    SerializeFromJson(tenants, RequestBuilder(ctx).Get(baseUrl(cluster) + "/tenants").Execute());

    // Get everything
    for (const auto& tenant : tenants) {
        const auto tnurl = baseUrl(cluster) + "/namespaces/" + tenant;
        vector<string> namespaces;
        try {
            SerializeFromJson(
                        namespaces,
                        RequestBuilder(ctx).Get(tnurl).Execute());
        } catch (const std::exception& ex) {
            LOG_WARN << cluster.logName() << ": Failed to access " << tnurl;
            continue;
        }

        for (const auto& ns : namespaces) {
            // ns contains "tenant/ns"

            const auto nspurl = baseUrl(cluster) + "/namespaces/" + ns;
            try {
                SerializeFromJson(
                            cluster.tenants[tenant].namespaces[ns].policies,
                            RequestBuilder(ctx).Get(nspurl).Execute());
            } catch (const std::exception& ex) {
                LOG_WARN << cluster.logName() << ": Failed to access " << nspurl;
                continue;
            }

            const auto nsurl = baseUrl(cluster) + "/persistent/" + ns;
            vector<string> topics;
            try {
                SerializeFromJson(
                            topics, RequestBuilder(ctx).Get(nsurl).Execute());
            } catch (const std::exception& ex) {
                LOG_WARN << cluster.logName() << ": Failed to access " << nsurl;
                continue;
            }

            for (const auto& topic : topics) {
                if (tfilter && !std::regex_search(topic, *tfilter)) {
                    continue;
                }

                const auto sturl = baseUrl(cluster) + "/persistent/" + stripPersistent(topic) + "/stats";
                try {
                    SerializeFromJson(cluster.tenants[tenant].namespaces[ns].topics[topic],
                            RequestBuilder(ctx)
                            .Get(sturl)
                            .Execute());
                } catch (const std::exception& ex) {
                    LOG_WARN << cluster.logName() << ": Failed to access " << sturl;
                    continue;
                }

                LOG_DEBUG << cluster.logName() << ": Got stats from topic " << topic;

                const Stats st = cluster.tenants[tenant].namespaces[ns].topics[topic];
                cluster.tenants[tenant].namespaces[ns].stats += st;
                cluster.tenants[tenant].stats += st;
                cluster.stats += st;
            }
        }
    }
}

void Engine::simpleSummary()
{
    for(const auto& [_, c] : clusters_) {
        cout << "Cluster " << c->name << ": " << strings(c->clusters) << endl;
        cout << "  Tenants: " << endl;
        for(const auto& [name, tenant] : c->tenants) {
            cout << "    " << name << " namespaces:" << endl;
            for(const auto& [nsname, ns] : tenant.namespaces) {
                cout << "      " << nsname << ' ' << strings(ns.policies.replication_clusters) << endl;
            }
        }

        cout << endl;
    }
}

void Engine::Cluster::setKubeconfig(const string &def)
{
    vector<string> args;
    boost::split(args, def, boost::is_any_of(","));

    if (args.size() >= 1) {
        origin = args.at(0);
    }

    if (args.size() >= 2) {
        name = args.at(1);
    }

    if (args.size() >= 3) {
        ns = args.at(2);
    }

    if (args.size() >= 4) {
        svcName = args.at(3);
    }
}

void Engine::Cluster::setUrl(const string &def)
{
    vector<string> args;
    boost::split(args, def, boost::is_any_of(","));

    if (args.size() >= 1) {
        url = args.at(0);
    }

    if (args.size() >= 2) {
        name = args.at(1);
    }
}


} // ns
