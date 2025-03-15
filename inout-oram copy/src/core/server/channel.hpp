#pragma once
#include "server/server.hpp"
#include "oram/common/block.hpp"

namespace channel {

template <typename EncryptedBucket, size_t EncryptedBucketSize = 0>
class PathORAMChannel {
    private:
        server::StorageServer<EncryptedBucket, EncryptedBucketSize> server_;
    
    public:
        PathORAMChannel(const server::ServerConfig &config) : server_(config) {}
        
        void write_bucket(ORBucketID id, EncryptedBucket EncBucket) { server_.write_bucket(EncBucket); }
        void write_buckets(std::map<ORBucketID, EncryptedBucket> EncBuckets) { server_.write_buckets(EncBuckets); }
        void read_bucket(const ORBucketID &id, EncryptedBucket EncBucket) { server_.read_bucket(id, EncBucket); }
        void read_buckets(std::vector<ORBucketID> &ids, std::vector<EncryptedBucket> &EncBuckets) { server_.read_buckets(ids, EncBuckets); }
};

} // namespace channel