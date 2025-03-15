#pragma once
#include <math.h>

#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "oram/common/block.hpp"
#include "server/channel.hpp"
#include "utils/crypto.hpp"
#include "utils/random_gen.hpp"
#include "threadpool.h"
#include "stopwatch.hpp"


using ORKey = uint32_t;

template <size_t B>
class PathORAMClient {

 public:
  bool setup_ = false, successful = false;
  inline static constexpr size_t BlockSize() { return sizeof(uint32_t) + B; }

  inline static constexpr size_t EncryptedBlockSize() {
    return utils::CiphertextLen(BlockSize());
  }

  inline static constexpr size_t BucketSize() {
    return sizeof(char) + Z * BlockSize();
  }

  inline static constexpr size_t EncryptedBucketSize() {
    return utils::CiphertextLen(BucketSize());
  }

    using TPathORAMChannel = std::shared_ptr<channel::PathORAMChannel<char *, PathORAMClient<B>::EncryptedBucketSize()>>;

  static std::optional<PathORAMClient *> Construct(size_t n, 
            TPathORAMChannel channel,
            utils::Key key) {
    // Initialize the ORAM
    auto o = new PathORAMClient(n, std::move(channel), key);
    if (o->successful) {
      return o;
    }

    return std::nullopt;
  }

  void ParInit(threadpool::threadpool_context *ctx, std::vector<common::Block<B>> &blocks, size_t n_threads) {
    if (!successful)
        return;
    
    run_setup_par(ctx, blocks, n_threads);
  }

  void Init(std::vector<common::Block<B>> &blocks) {
    if (!successful)
        return;
    
    Setup(blocks);
  }

  void getPathToLeaf(Leaf leaf, std::vector<ORBucketID> &path) {
    auto cur_id = leaf;

    // Traverse from the leaf to the root
    while (cur_id >= 0) {
      path.push_back(cur_id);

      if (cur_id == 0) { break; }

      cur_id = (cur_id - 1) / 2;  // Move to parent
      cache_.insert(cur_id);
    }
  }

  void Read(ORKey w, common::Block<B> &data) {
    bool found = false;
    this->Read(w);

    for (auto b : stash_) {
      if (b.key == w) {
        data.key = b.key;
        memcpy(data.val, b.val, B);
        found = true;
      }
    }

    if (!found) {
      throw std::runtime_error("Block not found");
    }
    return;
  }

  void Evict() {
    this -> evict();
  }

 protected:
  std::map<ORKey, Leaf> pos_map_;  // position map
  std::set<ORBucketID>
      cache_;  // Stores which nodes have been accessed so far (before eviction)
  std::vector<common::Block<B>> stash_;
  size_t n_, bs_, min_leaf_, max_stash_size_, l_;  // n_ = number of blocks, bs_ = block size, l_ = height of the tree
  size_t en_bs_, max_leaf_;   // Encrypted block size
  size_t bus_;     // Bucket size
  size_t en_bus_;  // Encrypted bucket size
  TPathORAMChannel channel_;
  utils::Key EK;
  std::mutex cache_mutex_;

  PathORAMClient(size_t n, 
        TPathORAMChannel channel,
        utils::Key key) : n_(n), channel_(std::move(channel)), EK(key) {
    bs_ = BlockSize();
    en_bs_ = EncryptedBlockSize();
    bus_ = BucketSize();
    en_bus_ = EncryptedBucketSize();
    l_ = std::ceil(log2(n_));
    max_stash_size_ = 2 * Z * l_;
    min_leaf_ = (1ULL << l_) - 1;
    max_leaf_ = min_leaf_ << 1;

    successful = true;
  }

  void Read(ORKey w) {
    Leaf leaf = pos_map_[w];
    std::vector<ORBucketID> path;
    getPathToLeaf(leaf, path);

    read_path(path);

    pos_map_[w] = min_leaf_ + random_gen::generateRandomNumber(n_);

  }

  void Write(ORKey w, uint8_t *data) {
    Leaf leaf = pos_map_[w];
    std::vector<ORBucketID> path;
    getPathToLeaf(leaf, path);

    read_path(path);

    for (auto b : stash_) {
      if (b.key == w) {
        memcpy(b.val, data, B);
      }
    }

    Leaf new_leaf = min_leaf_ + random_gen::generateRandomNumber(n_);
    pos_map_[w] = new_leaf;
  }


  
  struct par_setup_args {
    std::vector<common::Block<B>> &blocks;
    size_t n_threads;
    size_t thread_id;
    size_t start,end;
    PathORAMClient<B> *oram;
  };
  void ParSetup(void *args) {
    spdlog::info("\tthread_id:{}\n\tstart:{}\n\tend:{}", static_cast<par_setup_args *>(args)->thread_id, static_cast<par_setup_args *>(args)->start, static_cast<par_setup_args *>(args)->end);
    auto *setup_args = static_cast<par_setup_args *>(args);
    auto &[blocks, n_threads, thread_id, start, end, oram] = *setup_args;
    assert(oram);
    auto cache_start_ = 2*start;
    auto cache_end_ = 2*end;
    if (thread_id == n_threads - 1) {
      cache_end_ = 2*end-1;
    }

    for (size_t i = start; i < end; i++) {
      Leaf k = oram->min_leaf_ + random_gen::generateRandomNumber(n_);
      oram->pos_map_.insert({blocks[i].key, k});
      oram->stash_.push_back(blocks[i]);
    }

    for (size_t i = cache_start_; i < cache_end_; i++) {
      std::lock_guard<std::mutex> lock(oram->cache_mutex_);
      oram->cache_.insert(i);
    }
  }

  void run_setup_par(threadpool::threadpool_context_t *ctx, std::vector<common::Block<B>> &blocks, size_t n_threads) {
// #ifdef LOG
    spdlog::info("[PAR-SETUP]: \n n = {}\n num_threads = {}", blocks.size(), n_threads);
// #endif

    auto works_ = (threadpool::thread_work *)alloca(n_threads * sizeof(threadpool::thread_work));
    auto args_ = (par_setup_args *)alloca(n_threads * sizeof(par_setup_args));
    size_t work_count = 0;
    Stopwatch parsetup;

    parsetup.start();
    for(size_t tid = 0; tid < n_threads; tid++) {
      size_t start = (tid * blocks.size()) / n_threads;
      size_t end = ((tid + 1) * blocks.size()) / n_threads;

      new (&args_[work_count]) par_setup_args{blocks, n_threads, tid, start, end, this};

      if (tid == n_threads - 1) {
        this->ParSetup(&args_[work_count]);
      }
      else {
        works_[work_count] = threadpool::thread_work {
                              .type = threadpool::THREAD_WORK_SINGLE,
                              .single = {.func = [](void *arg) { 
                                          auto *args = static_cast<par_setup_args *>(arg);
                                          args->oram->ParSetup(arg); 
                                                  },
                                         .arg = &args_[work_count]}
                          };
        threadpool::thread_work_push(ctx, &works_[work_count]);
        work_count++;  
      }
    }

    for (size_t i = 0; i < work_count; i++) {
      spdlog::info("[PAR-SETUP]: waiting for thread {}", i);
      threadpool::thread_wait(ctx, &works_[i]);
    }
    spdlog::info("[PAR-SETUP] time elapsed for par {}", parsetup.elapsed_sec());

    // Evict the stash
    setup_ = true;
    evict();
  }

  void Setup(std::vector<common::Block<B>> &blocks) {
    Stopwatch setup;
    // Create random positions for each block
    setup.start();
    for (size_t i = 0; i < n_; i++) {
      Leaf k = this->min_leaf_ + random_gen::generateRandomNumber(n_);
      pos_map_.insert({blocks[i].key, k});
    }

    // Since eviction works with cache, we make cache contain all nodes of the tree
    for (size_t i = 0; i < 2*n_-1; i++) { cache_.insert(i); }

    // Push all the blocks into the stash
    for (auto b : blocks) {
      stash_.push_back(b);
    }
    spdlog::info("[SEQ-SETUP] time elapsed for seq {}", setup.elapsed_sec());

    // Evict the stash
    setup_ = true;
    evict();
  }

  void read_path(std::vector<ORBucketID> &ids) {
    // Read the Encrypted buckets from the server
    std::vector<char *> enc_buckets;
    for (auto id : ids) {
      char *en_bu = (char *)malloc(en_bus_ * sizeof(char));
      enc_buckets.push_back(en_bu);
    }
    channel_->read_buckets(ids, enc_buckets);

    // Iterate over the read buckets
    for (auto en_bu : enc_buckets) {
      // Decrypt each bucket
      char *bu_ser = (char *)malloc(bus_ * sizeof(char));
      auto dec_ = utils::Decrypt(en_bu, en_bus_, EK, bu_ser);

      if (dec_ != bus_) {
        throw std::runtime_error("Failed to decrypt bucket");
        exit(1);
      }

      // Deserialize the buckets
      common::Bucket<B> bu;
      bu.deserialize(bu_ser);

      // If the bucket is empty, continue
      if (bu.flags_ == 0) {
        continue;
      }

      // Otherwise, add the bucket's blocks to the stash.
      for (int i = 0; i < bu.flags_; i++) {
        stash_.push_back(bu.blocks_[i]);
      }
    }

    enc_buckets.clear();
  }

  void evict() {
    // If the stash is empty, return
    if (stash_.empty()) {
      return;
    }

    std::map<ORBucketID, common::Bucket<B>> to_write;

    for (auto level = l_; level >= 0; level--) {
      // Computing the cached nodes in the current level
      auto min_level = (1ULL << level) - 1;
      auto max_level = min_level << 1;
      auto low = cache_.lower_bound(min_level);
      auto high = cache_.lower_bound(max_level);

#ifdef LOG
      std::stringstream ss;
      ss << "Evicting level " << level << std::endl
            << "Low: " << *low << std::endl
            << "High: " << *high << std::endl;
      spdlog::info(ss.str());
#endif

      // Initializing the buckets with the corresponding bucket ids.
      for (ORBucketID id = *low; id <= *high; id++) {
        to_write[id] = common::Bucket<B>();
      }

      // Now, we need to place the blocks in the stash into the buckets
      // they can reside into.
      for (size_t block_idx = 0; block_idx < stash_.size(); block_idx++) {
        auto b = stash_[block_idx];
        std::vector<ORBucketID> path;
        auto leaf = pos_map_[b.key];
        assert(leaf >= min_leaf_ && leaf <= max_leaf_);
        getPathToLeaf(leaf, path);

        assert(path.size() == l_ + 1);

        auto cur_id = path[level];

        // If the bucket is not in the to_write map, continue
        if (to_write.find(cur_id) == to_write.end()) {
          continue;
        }

        // If the bucket is full, continue
        auto flags = to_write[cur_id].flags_;
        if (flags == Z) {
          continue;
        }

        // Add the block to the bucket
        to_write[cur_id].blocks_[flags].key = b.key;
        std::memcpy(to_write[cur_id].blocks_[flags].val, b.val, B);
        to_write[cur_id].flags_++;
        stash_.erase(stash_.begin() + block_idx);
      }

      if (level == 0) { break; /* root is done we can exit */ }
    }

    if (stash_.size() > max_stash_size_ && setup_ == false) {
      throw std::runtime_error("Stash size exceeded");
      exit(1);
    }

    // Now we have constructed all the buckets that need to be written
    // The only thing that's left is to serialize them and encrypt them
    // before sending them to the channel.
    std::map<ORBucketID, char *> to_send;
    for (auto &it : to_write) {
      auto &[bucket_offset, bucket] = it;
      char *bu_ser = (char *)malloc(PathORAMClient<B>::BucketSize() * sizeof(char));
      bucket.serialize(bu_ser);

      char *en_bu = (char *)malloc(PathORAMClient<B>::EncryptedBucketSize() * sizeof(char));
      auto suc = utils::Encrypt(bu_ser, bus_, EK, en_bu);

      if (!suc) {
        throw std::runtime_error("Failed to encrypt bucket");
        exit(1);
      }

      to_send[bucket_offset] = en_bu;
      free(bu_ser);
    }

    // Send the encrypted buckets to the server
    channel_->write_buckets(to_send);
    to_send.clear();
    cache_.clear();
  }
};