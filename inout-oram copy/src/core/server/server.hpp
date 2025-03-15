#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <cassert>
#include <filesystem>

#include "oram/common/block.hpp"

namespace server {
// Server configuration
struct ServerConfig {
    enum class StorageType { Memory, Disk };
    StorageType type;
    std::string diskDirectory;
};

// Storage strategy interface with template parameter
template<typename EncryptedBucket, size_t EncryptedBucketSize = 0>
class BucketStorage {
public:
    virtual ~BucketStorage() = default;
    virtual void write_bucket(uint32_t id, const EncryptedBucket bucket) = 0;
    virtual void read_bucket(uint32_t id, EncryptedBucket res) = 0;
    virtual void read_buckets(std::vector<uint32_t> &ids, std::vector<EncryptedBucket> &res) = 0;
    virtual void write_buckets(std::map<uint32_t, EncryptedBucket> &buckets) = 0;
};

// Memory-based storage implementation
template<typename EncryptedBucket, size_t EncryptedBucketSize = 0>
class MemoryStorage : public BucketStorage<EncryptedBucket, EncryptedBucketSize> {
private:
    std::unordered_map<uint32_t, EncryptedBucket> buckets;

public:
    void write_bucket(uint32_t id, const EncryptedBucket bucket) override {
      if (!bucket) {
          throw std::invalid_argument("[WRITE_BUCKET] Bucket must not be null");
      }
      if (!buckets[id]) {
        buckets[id] = static_cast<char*>(malloc(EncryptedBucketSize * sizeof(char)));
      }
      std::copy(bucket, bucket + EncryptedBucketSize, buckets[id]);
    }

    void write_buckets(std::map<uint32_t, EncryptedBucket> &buckets) override{
        for (const auto& [id, bucket] : buckets) {
            write_bucket(id, bucket);
        }
    }

    void read_bucket(uint32_t id, EncryptedBucket res) override {
        if (!res) {
            throw std::invalid_argument("[READ_BUCKET] Result buffer must not be null");
        }
        auto it = buckets[id];

        std::memcpy(res, it, EncryptedBucketSize);
    }

    void read_buckets(std::vector<ORBucketID> &ids, std::vector<EncryptedBucket> &res) override {
        for (size_t i = 0; i < ids.size(); i++) {
            read_bucket(ids[i], res[i]);
        }
    }

};

// Disk-based storage implementation
template<typename EncryptedBucket, size_t EncryptedBucketSize>
class DiskStorage : public BucketStorage<EncryptedBucket, EncryptedBucketSize> {
private:
    std::string filename;
    std::fstream file;

    void openFile() {
        if(!std::filesystem::exists(filename)) {
            std::ofstream tempFile(filename);
            tempFile.close();
        }
        this->file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    }

public:
    explicit DiskStorage(const std::string& path) : filename(path) {
        openFile();
        if (!file.is_open()) {
            throw std::runtime_error("\n[DISK STORAGE] Failed to open storage file: " + path);
        }
    }

    ~DiskStorage() {
        if (file.is_open()) {
            file.close();
        }
    }

    void write_bucket(uint32_t id, const EncryptedBucket bucket) override {
        assert(bucket);
        file.seekp(id * EncryptedBucketSize);
        if (!file.good()) {
            throw std::runtime_error("Failed to seek to position for bucket: " + 
                                   std::to_string(id));
        }

        file.write(bucket, EncryptedBucketSize);
        if (!file.good()) {
            throw std::runtime_error("Failed to write bucket: " + 
                                   std::to_string(id));
        }

        file.flush();
    }

    void read_bucket(uint32_t id, EncryptedBucket res) override {
        assert(res);
        file.seekg(id * EncryptedBucketSize);
        if (!file.good()) {
            throw std::runtime_error("Failed to seek to position for bucket: " + 
                                   std::to_string(id));
        }

        file.read(res, EncryptedBucketSize);
        if (file.gcount() != EncryptedBucketSize) {
            throw std::runtime_error("Failed to read complete bucket: " + 
                                   std::to_string(id));
        }
    }
    
    void write_buckets(std::map<ORBucketID, EncryptedBucket> &buckets) override {
        for (const auto& [id, bucket] : buckets) {
            write_bucket(id, bucket);
        }
    }

    void read_buckets(std::vector<ORBucketID> &ids, std::vector<EncryptedBucket> &res) override {
        for (size_t i = 0; i < ids.size(); i++) {
            read_bucket(ids[i], res[i]);
        }
    }
};

// Main server class
template<typename EncryptedBucket, size_t EncryptedBucketSize = 0>
class StorageServer {
private:
    std::unique_ptr<BucketStorage<EncryptedBucket, EncryptedBucketSize>> storage;
    ServerConfig config_;

public:
    explicit StorageServer(const ServerConfig& config) : config_(config) {
        std::cout << "[SERVER] Initializing Storage Server, with Encrypted Bucket/Value Size: " << EncryptedBucketSize << std::endl;
        switch (config.type) {
            case ServerConfig::StorageType::Memory:
                storage = std::make_unique<MemoryStorage<EncryptedBucket, EncryptedBucketSize>>();
                break;
            case ServerConfig::StorageType::Disk:
                if (config.diskDirectory.empty()) {
                    throw std::runtime_error("Disk directory must be specified for disk storage");
                }
                if constexpr (EncryptedBucketSize == 0) {
                    throw std::runtime_error("Bucket size must be specified for disk storage");
                }
                storage = std::make_unique<DiskStorage<EncryptedBucket, EncryptedBucketSize>>(
                    config.diskDirectory
                );
                break;
        }
    }

    void write_bucket(uint32_t id, const EncryptedBucket& bucket) {
        storage->write_bucket(id, bucket);
    }

    void write_buckets(std::map<uint32_t, EncryptedBucket> &buckets) {
        storage->write_buckets(buckets);
    }

    void read_bucket(uint32_t id, EncryptedBucket res) {
        return storage->read_bucket(id, res);
    }

    void read_buckets(std::vector<ORBucketID> &ids, std::vector<EncryptedBucket> &res) {
        return storage->read_buckets(ids, res);
    }
};

} // namespace server
