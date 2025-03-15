// after this is server.cpp
#include <spdlog/spdlog.h>

#include <cassert>
#include <iostream>
#include <map>
#include <array>  // Added for std::array
#include <iomanip> // Added for std::setw, std::setfill

#include "oram/path_oram/path_oram.hpp"
#include "server/server.hpp"
#include <unordered_map>
// #include <seal/seal.h>  // Include Microsoft SEAL
const size_t B = 8;
using ExampleEncryptedBucket = char *;
using ExampleBucket = common::Bucket<B>;
using ExampleBlock = common::Block<B>;
using namespace server;

class ObliviousDictionary {
public:
    void Insert(std::string keyword, uint32_t index, uint32_t count) {
        dictionary_[keyword] = std::make_pair(index, count);
    }

    std::pair<uint32_t, uint32_t> Search(std::string keyword) {
        return dictionary_[keyword];  // Ensure this is an oblivious lookup
    }
private:
    std::unordered_map<std::string, std::pair<uint32_t, uint32_t>> dictionary_;
};

void microbenchmark_block_move() {
  // Create test block
  ExampleBlock block;
  uint8_t temp[B];
  std::memset(temp, 0xAA, B);
  std::memset(block.val, 0xAA, B);
  block.key = 33;

  // Move block
  ExampleBlock moved = std::move(block);

  // Verify data
  assert(block.key == 0);  // 2^32 - 1, thats the value of -1 in uint32_t
  assert(std::memcmp(moved.val, temp, B) == 0);
  assert(moved.key == 33);

  std::cout << "[PASSED] Block Move" << std::endl;
}

void microbenchmarks_block() {
  // Create test block
  for (size_t i = 0; i < 100; i++) {
    ExampleBlock block;
    std::memset(block.val, 0xAA + random_gen::generateRandomNumber(1000), B);
    block.key = random_gen::generateRandomNumber(100);

    // Serialize block
    char *buf = (char *)malloc(PathORAMClient<B>::BlockSize() * sizeof(char));
    block.serialize(buf);

    // Deserialize block
    ExampleBlock deserialized;
    deserialized.deserialize(buf);

    // Verify data
    assert(block.key == deserialized.key);
    assert(std::memcmp(block.val, deserialized.val, B) == 0);
  }
  std::cout << "[PASSED] Block Serialization" << std::endl;
}

void microbenchmarks_bucket() {
  // Create test bucket
  ExampleBlock b1, b2;
  std::memset(b1.val, 0xAA, B);
  b1.key = 33;
  std::memset(b2.val, 0xBB, B);
  b2.key = 44;

  ExampleBucket bucket;
  bucket.flags_ = 2;
  bucket.blocks_[0].key = b1.key;
  bucket.blocks_[1].key = b2.key;
  std::memcpy(bucket.blocks_[0].val, b1.val, PathORAMClient<B>::BlockSize());
  std::memcpy(bucket.blocks_[1].val, b2.val, PathORAMClient<B>::BlockSize());
  // Serialize bucket
  char *buf =
      (char *)malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char));
  bucket.serialize(buf);

  // Deserialize bucket
  ExampleBucket deserialized;
  deserialized.deserialize(buf);

  // Verify data
  assert(bucket.flags_ == deserialized.flags_);
  assert(bucket.blocks_[0].key == deserialized.blocks_[0].key);
  assert(std::memcmp(bucket.blocks_[0].val, deserialized.blocks_[0].val, B) ==
         0);
  assert(std::memcmp(bucket.blocks_[1].val, deserialized.blocks_[1].val, B) ==
         0);
  assert(bucket.blocks_[1].key == deserialized.blocks_[1].key);

  std::cout << "[PASSED] Bucket Serialization" << std::endl;
}

void test_memory_storage() {
  auto key = utils::GenerateKey();
  // Configure server for memory storage
  ServerConfig config;
  config.type = ServerConfig::StorageType::Memory;

  // Create server instance
  StorageServer<ExampleEncryptedBucket,
                PathORAMClient<B>::EncryptedBucketSize()>
      server(config);

  // Create test bucket
  ExampleBlock b1, b2;

  std::memset(b1.val, 0xAA, B);
  b1.key = 33;

  std::memset(b2.val, 0xBB, B);
  b2.key = 44;

  ExampleBucket bu;
  bu.flags_ = 2;
  bu.blocks_[0].key = b1.key;
  bu.blocks_[1].key = b2.key;
  std::memset(bu.blocks_[0].val, 0xAA, B);
  std::memset(bu.blocks_[1].val, 0xBB, B);

  // Serialize bucket (imitating the eviction of path_oram)
  char *bu_ser = (char *)malloc(PathORAMClient<B>::BucketSize() * sizeof(char));
  bu.serialize(bu_ser);
  char *en_bu =
      (char *)malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char));
  auto success =
      utils::Encrypt(bu_ser, PathORAMClient<B>::BucketSize(), key, en_bu);
  if (!success) {
    std::cerr << "[MEMORY] Encryption failed!" << std::endl;
    return;
  }

  assert(bu.blocks_[0].key == b1.key);
  assert(bu.blocks_[1].key == b2.key);

  // Store bucket
  server.write_bucket(3, en_bu);

  assert(bu.blocks_[0].key == b1.key);
  // Read bucket
  ExampleEncryptedBucket retrieved = static_cast<char *>(
      malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char)));
  server.read_bucket(3, retrieved);

  // Verify Encrypted data
  assert(std::memcmp(en_bu, retrieved,
                     PathORAMClient<B>::EncryptedBucketSize()) == 0);

  // Verify Bucket data
  char *dec_bu = (char *)malloc(PathORAMClient<B>::BucketSize() * sizeof(char));
  auto plen = utils::Decrypt(
      retrieved, PathORAMClient<B>::EncryptedBucketSize(), key, dec_bu);
  ExampleBucket verificator;
  verificator.deserialize(dec_bu);
  assert(plen == PathORAMClient<B>::BucketSize());
  assert(bu.flags_ == verificator.flags_);
  assert(bu.blocks_[0].key == verificator.blocks_[0].key);
  assert(std::memcmp(bu.blocks_[0].val, verificator.blocks_[0].val, B) == 0);
  assert(std::memcmp(bu.blocks_[1].val, verificator.blocks_[1].val, B) == 0);
  assert(bu.blocks_[1].key == verificator.blocks_[1].key);
  std::cout << "[PASSED] Memory Storage Test" << std::endl;
}

void printBufferHex(const char *buffer, size_t size, const std::string &label) {
  std::cout << label << " (" << size << " bytes): ";
  for (size_t i = 0; i < size; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(static_cast<unsigned char>(buffer[i])) << " ";
    // Add newline every 16 bytes for readability
    if ((i + 1) % 16 == 0) std::cout << std::endl;
  }
  std::cout << std::dec << std::endl;
}

void test_disk_storage() {
  // Configure server for disk storage
  auto key = utils::GenerateKey();
  ServerConfig config;
  config.type = ServerConfig::StorageType::Disk;
  config.diskDirectory =
      "/Users/apostolosmavrogiannakis/Desktop/PhD-prep/inout-oram/src/storage/"
      "test-disk-storage";

  // Create server instance with specified bucket size
  StorageServer<ExampleEncryptedBucket,
                PathORAMClient<B>::EncryptedBucketSize()>
      server(config);

  // Create test buckets
  std::vector<ExampleBucket> buckets;
  // Fixed: changed from std::vector<uint8_t[B]> to std::vector<std::array<uint8_t, B>>
  std::vector<std::array<uint8_t, B>> values;
  
  for (int i = 0; i < 100; i++) {
    ExampleBucket bucket;
    auto f_ = random_gen::generateRandomNumber(Z);
    bucket.flags_ = f_;
    for (int k = 0; k < Z; k++) {
      std::memset(bucket.blocks_[k].val, 0xAA + i, B);
      bucket.blocks_[k].key = random_gen::generateRandomNumber(100);
    }
    // std::memset(bucket, 0xAA+i, sizeof(ExampleBucket));
    buckets.push_back(bucket);
  }

  for (int i = 0; i < 100; i++) {
    // Serialize bucket
    char *bu_ser =
        (char *)malloc(PathORAMClient<B>::BucketSize() * sizeof(char));
    buckets[i].serialize(bu_ser);
    char *en_bu =
        (char *)malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char));
    auto success =
        utils::Encrypt(bu_ser, PathORAMClient<B>::BucketSize(), key, en_bu);
    if (!success) {
      std::cerr << "[DISK] Encryption failed!" << std::endl;
      return;
    }
    // Store buckets
    server.write_bucket(i, en_bu);

    // Read buckets
    ExampleEncryptedBucket retrieved = static_cast<char *>(
        malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char)));
    server.read_bucket(i, retrieved);
    if (i == 0) {
      size_t size = PathORAMClient<B>::EncryptedBucketSize();
      printBufferHex(en_bu, size, "Original buffer");
      printBufferHex(retrieved, size, "Retrieved buffer");
    }
    assert(std::memcmp(en_bu, retrieved,
                       PathORAMClient<B>::EncryptedBucketSize()) == 0);

    char *dec_bu =
        (char *)malloc(PathORAMClient<B>::BucketSize() * sizeof(char));
    auto plen = utils::Decrypt(
        retrieved, PathORAMClient<B>::EncryptedBucketSize(), key, dec_bu);
    ExampleBucket verificator;
    verificator.deserialize(dec_bu);

    // Verifications
    assert(plen == PathORAMClient<B>::BucketSize());

    assert(buckets[i].flags_ == verificator.flags_);

    for (int k = 0; k < Z; k++) {
      assert(buckets[i].blocks_[k].key == verificator.blocks_[k].key);
      assert(std::memcmp(buckets[i].blocks_[k].val, verificator.blocks_[k].val,
                         B) == 0);
    }

    free(bu_ser);
    free(en_bu);
    free(retrieved);
  }

  buckets.clear();

  std::cout << "[PASSED] Disk Storage Test" << std::endl;
}

int main() {
  microbenchmark_block_move();
  microbenchmarks_block();
  microbenchmarks_bucket();
  test_memory_storage();
  // test_disk_storage();
  return 0;
}