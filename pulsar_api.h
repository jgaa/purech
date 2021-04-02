#pragma once

#include <boost/fusion/adapted.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/include/define_struct.hpp>
#include <boost/process.hpp>

struct Stats {
    Stats& operator+=(const Stats& v) {
        msgRateIn += v.msgRateIn;
        msgThroughputIn += v.msgThroughputIn;
        msgRateOut += v.msgRateOut;
        msgThroughputOut += v.msgThroughputOut;
        return *this;
    }

    double msgRateIn = {};
    double msgThroughputIn = {};
    double msgRateOut = {};
    double msgThroughputOut = {};
};

struct Publisher {
    double msgRateIn = {};
    double msgThroughputIn = {};
    double averageMsgSize = {};
    uint64_t producerId = {};
    std::string address;
    std::string clientVersion;
    std::string connectedSince;
    std::string producerName;
};

struct Consumer {
    double msgRateOut = {};
    double msgThroughputOut = {};
    double msgRateRedeliver = {};
    std::string consumerName;
    int availablePermits = {};
    int unackedMessages = {};
    bool blockedConsumerOnUnackedMsgs = false;
    std::string address;
    std::string clientVersion;
    std::string connectedSince;
};

struct Replication : public Stats {
    double msgRateExpired = {};
    int replicationBacklog = {};
    bool connected = false;
    int replicationDelayInSeconds = {};
    std::string outboundConnection;
    std::string outboundConnectedSince;
};

struct Subscription {
    using consumers_t = std::vector<Consumer>;

    double msgRateOut = {};
    double msgThroughputOut = {};
    double msgRateRedeliver = {};
    uint64_t msgBacklog = {};
    bool blockedSubscriptionOnUnackedMsgs = false;
    uint64_t unackedMessages = {};
    std::string type;
    std::string activeConsumerName;
    double msgRateExpired = {};
    consumers_t consumers;
};

struct PersistentTopicStats : public Stats {
    using publishers_t = std::deque<Publisher>;
    using subscriptions_t = std::map<std::string, Subscription>;
    using replication_t = std::map<std::string, Replication>;
    double averageMsgSize = {};
    double storageSize = {};
    publishers_t publishers;
    subscriptions_t subscriptions;
    replication_t replication;
    std::string deduplicationStatus;
};

struct  NamespacePolicies {
    using strlist_t = std::vector<std::string>;
    strlist_t replication_clusters;
};

struct Namespace {
    using topics_t = std::map<std::string /* topic */, PersistentTopicStats>;
    Stats stats;
    topics_t topics;
    NamespacePolicies policies;
};

struct Tenant {
    using namespaces_t = std::map<std::string /* tenant */, Namespace>;
    Stats stats;
    namespaces_t namespaces;
};

struct Location {
    using tenants_t = std::map<std::string /* tenant */, Tenant>;
    Stats stats;
    tenants_t tenant;
};

BOOST_FUSION_ADAPT_STRUCT(Publisher,
    (double, msgRateIn)
    (double, msgThroughputIn)
    (double, averageMsgSize)
    (uint64_t, producerId)
    (std::string, address)
    (std::string, clientVersion)
    (std::string, connectedSince)
    (std::string, producerName))

BOOST_FUSION_ADAPT_STRUCT(Consumer,
    (double, msgRateOut)
    (double, msgThroughputOut)
    (double, msgRateRedeliver)
    (std::string, consumerName)
    (int, availablePermits)
    (int, unackedMessages)
    (bool, blockedConsumerOnUnackedMsgs)
    (std::string, address)
    (std::string, clientVersion)
    (std::string, connectedSince))

BOOST_FUSION_ADAPT_STRUCT(Subscription,
    (double, msgRateOut)
    (double, msgThroughputOut)
    (double, msgRateRedeliver)
    (uint64_t, msgBacklog)
    (bool, blockedSubscriptionOnUnackedMsgs)
    (uint64_t, unackedMessages)
    (std::string, type)
    (std::string, activeConsumerName)
    (double, msgRateExpired)
    (Subscription::consumers_t, consumers))

BOOST_FUSION_ADAPT_STRUCT(Replication,
    (double, msgRateIn)
    (double, msgThroughputIn)
    (double, msgRateOut)
    (double, msgThroughputOut)
    (double, msgRateExpired)
    (int, replicationBacklog)
    (bool, connected)
    (int, replicationDelayInSeconds)
    (std::string, outboundConnection)
    (std::string, outboundConnectedSince))

BOOST_FUSION_ADAPT_STRUCT(PersistentTopicStats,
    (double, msgRateIn)
    (double, msgThroughputIn)
    (double, msgRateOut)
    (double, msgThroughputOut)
    (double, averageMsgSize)
    (double, storageSize)
    (PersistentTopicStats::publishers_t, publishers)
    (PersistentTopicStats::subscriptions_t, subscriptions)
    (PersistentTopicStats::replication_t, replication)
    (std::string, deduplicationStatus))

BOOST_FUSION_ADAPT_STRUCT(NamespacePolicies,
    (NamespacePolicies::strlist_t, replication_clusters))
