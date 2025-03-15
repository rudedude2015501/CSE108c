#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <filesystem>
#include <spdlog/spdlog.h>

#include "oram/path_oram/path_oram.hpp"
#include "server/channel.hpp"
#include "server/server.hpp"
#include "pthread_threadpool.hpp"
#include "threadpool.h"
#include "stopwatch.hpp"

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

// TODO: Use toml to configure the tests
const size_t B = 8;
const size_t n = 1ULL << 10;
const size_t n_threads = 4;

using ExampleEncryptedBucket = char *;
using ExampleBucket = common::Bucket<B>;
using ExampleBlock = common::Block<B>;
const size_t ExampleEncryptedBucketSize = PathORAMClient<B>::EncryptedBucketSize();

int main(int argc, char **argv) {
  // Configuration of the test
  auto key = utils::GenerateKey();
  server::ServerConfig config;
  
  config.type = server::ServerConfig::StorageType::Disk;
  
  // First, ensure any existing file is removed
  std::string storage_path = "/Users/abbykaur/Downloads/inout-oram copy/src/build/storage/test-path-oram";
  system(("rm -rf \"" + storage_path + "\"").c_str());
  
  // Create parent directory
  std::string parent_dir = "/Users/abbykaur/Downloads/inout-oram copy/src/build/storage";
  try {
    std::filesystem::create_directories(parent_dir);
  } catch (const std::exception& e) {
    // If directory already exists, that's fine
    spdlog::info("Note: Parent directory already exists: {}", parent_dir);
  }
  
  config.diskDirectory = storage_path;
  spdlog::info("Setting storage path to: {}", config.diskDirectory);

  // Create server and channel instance with specified bucket size
  std::shared_ptr<channel::PathORAMChannel<ExampleEncryptedBucket, ExampleEncryptedBucketSize>>
      channel = std::make_shared<channel::PathORAMChannel<ExampleEncryptedBucket, ExampleEncryptedBucketSize>>(config);

  std::optional<PathORAMClient<B> *> opt_oram = PathORAMClient<B>::Construct(n, std::move(channel), key);
  if (!opt_oram.has_value()) {
    std::cerr << "Failed to initialize ORAM" << std::endl;
    return 1;
  }

  PathORAMClient<B> *oram = opt_oram.value();
  std::vector<common::Block<B>> blocks;
  for (size_t i = 0; i < n; i++) {
    blocks.push_back(common::Block(i, random_gen::GenRandBytes<B>()));
  }

  { // Sequential mode
    Stopwatch seq;
    // Doing the initialization with a vector of blocks to reduce the communication round of initialization.
    spdlog::info("SeqInit ORAM with {} blocks", n);
    seq.start();
    oram->Init(blocks);
    spdlog::info("SeqInit took {} seconds", seq.elapsed_sec());

    common::Block<B> data;
    oram->Read(0, data);
    oram->Evict();
    spdlog::info("Evicting after Read");
    assert(data.key == blocks[0].key);
    assert(std::memcmp(data.val, blocks[0].val, B) == 0);
    
    // Clean up using quotes to handle spaces in path
    int result_code = system(("rm -rf \"" + storage_path + "\"").c_str());
    spdlog::info("Removing test-path-oram files with result code {}", result_code);
  }

  { // Parallel mode
    spdlog::info("SeqInit ORAM with {} threads", n_threads);
    
    PThreadThreadpool threadpool(n_threads);
    
    Stopwatch par;
    par.start();
    oram->ParInit(threadpool.get_context(), blocks, n_threads);
    spdlog::info("ParInit took {} seconds", par.elapsed_sec());

    common::Block<B> data;
    oram->Read(0, data);
    oram->Evict();
    spdlog::info("Evicting after read");
    assert(data.key == blocks[0].key);
    assert(std::memcmp(data.val, blocks[0].val, B) == 0);
    
    // Clean up using quotes to handle spaces in path
    int result_code = system(("rm -rf \"" + storage_path + "\"").c_str());
    spdlog::info("Removing test-path-oram files with result code {}", result_code);
  }

  return 0;
}