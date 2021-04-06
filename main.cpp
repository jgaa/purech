// We use the example from the main readme file

#include <filesystem>
//#include <iosfwd>
#include <locale>

#include <boost/algorithm/string.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/program_options.hpp>

#include "pulsar.h"

using namespace std;
using namespace purech;
namespace logging = boost::log;

int main(int argc, char* argv[]) {
    namespace logging = boost::log;
    std::string log_level;

    try {
        locale loc("");
    } catch (const std::exception& e) {
        std::cout << "Locales in Linux are fundamentally broken. Never worked. Never will. Overriding the current mess with LC_ALL=C" << endl;
        setenv("LC_ALL", "C", 1);
    }

    namespace po = boost::program_options;
    po::options_description general("Options");

    Config config;

    general.add_options()("help,h", "Print help and exit")
            ("log-level,l", po::value<string>(&log_level)->default_value("info"),
             "Log-level to use; one of 'info', 'debug', 'trace'")
            //("hide-idle", "Hide idle objects")
            //("hide-streams,S", "Hide individual streams")
            //("hide-stats", "Hide statistics")
            //("diagnostics-only,d", "Show only diqgnostics output")
            //("show-no-publishers", "Show 'no-publishers' in problems report")
            //("problems-only,p", "No reports - just look for problems.")
            //("streams-report,s", "Just list all the streams without all the details.")
            ("topic-filter,f", po::value<string>(&config.topicFilter))
            ("service-name,N", po::value<string>(&config.brokerSvcName)->default_value(config.brokerSvcName))
            ("local-port,P", po::value<uint16_t>(&config.localPort)->default_value(config.localPort))
            ("namespace,n", po::value<string>(&config.ns)->default_value(config.ns))
            ;

    po::options_description hidden("Hidden options");
    hidden.add_options()("targets",
                         po::value<decltype(config.clusters)>(&config.clusters),
                         "url|kubeconfig,cluster-name[,namespace[,brokerSvcName]]");

    po::options_description cmdline_options;
    po::positional_options_description kfo;
    cmdline_options.add(general).add(hidden);
    kfo.add("targets", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
              .options(cmdline_options)
              .positional(kfo)
              .run(),
              vm);

    po::notify(vm);

    if (vm.count("help")) {
        std::cout << filesystem::path(argv[0]).stem().string() << " [options]";
        std::cout << cmdline_options << std::endl;
        return -1;
    }

    config.hideIdle = vm.count("hide-idle") > 0;
    //config.hideStats = vm.count("hide-stats") > 0;
    config.hideStreams = vm.count("hide-streams") > 0;
    //config.hideNoPublishers = vm.count("show-no-publishers") == 0;
    //config.diagnosticsOnly = vm.count("diagnostics-only") > 0;

    //  if (vm.count("streams-report")) {
    //    config.showStreamsReport = true;
    //    config.showActivityReport = false;
    //  }

    //  if (vm.count("problems-only")) {
    //    config.showStreamsReport = false;
    //    config.showActivityReport = false;
    //  }

    auto llevel = logfault::LogLevel::INFO;
    if (log_level == "debug") {
        llevel = logfault::LogLevel::DEBUGGING;
    } else if (log_level == "trace") {
        llevel = logfault::LogLevel::TRACE;
    } else if (log_level == "info") {
        ;  // Do nothing
    } else {
        std::cerr << "Unknown log-level: " << log_level << endl;
        return -1;
    }

    logfault::LogManager::Instance().AddHandler(
                make_unique<logfault::StreamHandler>(clog, llevel));


    if (config.clusters.empty()) {
        if (const auto kc = std::getenv("KUBECONFIG")) {
            boost::split(config.clusters, kc, boost::is_any_of(":"));
        }
    }

    if (config.clusters.empty()) {
        BOOST_LOG_TRIVIAL(error)
                << "No kubefiles specified. " << endl
                << "Please set KUBECONFIG to point to your kubefiles.";
    }

    try {
        Engine engine{config};
        engine.run();
    } catch (const exception& ex) {
        LOG_ERROR << "Caught exception from run: " << ex.what();
    }

    return 0;
}
