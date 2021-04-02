#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "restc-cpp/restc-cpp.h"
#include "logfault/logfault.h"
#include "pulsar_api.h"

#define LOG_ERROR   LFLOG_ERROR
#define LOG_WARN    LFLOG_WARN
#define LOG_INFO    LFLOG_INFO
#define LOG_DEBUG   LFLOG_DEBUG
#define LOG_TRACE   LFLOG_TRACE

namespace purech {

struct PrcCtx;

struct Config {
  std::vector<std::string> clusters; // url|kubeconfig,cluster-name[,namespace[,brokerSvcName]]
  uint16_t localPort = 9123;
  bool diagnosticsOnly = false;
  bool hideIdle = false;
  //bool hideStats = false;
  bool hideStreams = false;
  //bool hideNoPublishers = true;
  //bool showStreamsReport = false;
  //bool showActivityReport = true;
  std::string topicFilter;
  std::string brokerSvcName = "pulsar-broker";
  std::string ns;
};

class Engine {
public:
    struct Cluster {
        using tenants_t = std::map<std::string /* tenant */, Tenant>;

        void setKubeconfig(const std::string& def);
        void setUrl(const std::string& def);

        std::string origin; // url or kubefile
        std::string ns; // if port-forwarding
        std::string svcName; // if port-forwarding
        std::string url; // Url used by the rest client
        std::string name;
        std::vector<std::string> clusters; // Clusters know in this location
        tenants_t tenants; // Tenants in this region
        Stats stats;

        std::string logName() const {
            return name;
        }
    };


    Engine(const Config& config);

    void run();
private:
    void prepare();
    void processCluster(Cluster& cluster, restc_cpp::Context& ctx);
    void simpleSummary();

    const Config& config_;
    std::map<std::string_view, std::shared_ptr<Cluster>> clusters_;
    std::unique_ptr<restc_cpp::RestClient> client_;
    std::vector<std::shared_ptr<PrcCtx>> processes_;
};

} // ns
