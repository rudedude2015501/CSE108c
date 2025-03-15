#pragma once
#include "oram/path_oram/path_oram.hpp"
#include "server/channel.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

template <size_t B>
class PathORAMLBClient : public PathORAMClient<B> {
    public:
    #define LPP 4
    size_t ll_; // height of the large bucket tree
    size_t large_bucket_size;
    using PathORAMClient<B>::pos_map_;
    using PathORAMClient<B>::stash_;
    using PathORAMClient<B>::n_;
    using PathORAMClient<B>::min_leaf_;
    using PathORAMClient<B>::cache_;
    using ORVirtualBucketID = uint32_t;
    using ORVirtualBucketOffset = uint32_t;
    using VirtualPositionID = uint32_t; // virtual position = virtual large bucket id + offset in the large bucket

    inline static constexpr size_t EncryptedLargeBucketSize() {
        return ((1 << LPP) - 1) * PathORAMClient<B>::EncryptedBucketSize();
    }

    using TPathORAMLBChannel = std::shared_ptr<channel::PathORAMChannel<char *, PathORAMLBClient<B>::EncryptedLargeBucketSize()>>;

    using u64 = uint64_t;

    static std::optional<PathORAMLBClient<B> *> Construct(size_t n, 
            TPathORAMLBChannel channel,
            utils::Key key) {
        // Initialize the ORAM
        std::cout << "[PATH ORAMLB] Constructing ORAM with n = " << n << std::endl
                << "\tPayload/Value size = " << B << std::endl
                << "\tEncrypted block size = " << PathORAMClient<B>::EncryptedBlockSize() << std::endl
                << "\tBucket size = " << PathORAMClient<B>::BucketSize() << std::endl;
        auto o = new PathORAMLBClient<B>(n, std::move(channel), key);
        if (o->successful) {
            return o;
        }

        return std::nullopt;
    }
    

    void Init(std::vector<common::Block<B>> blocks) { this->Setup(blocks);}
 
    void Access(Keyword w, uint8_t *data, bool write) {
        if (write) {
            this->Write(w, data);
        } else {
            if (!data) {
                data = new uint8_t[B];
            }
            this->Read(w, data);
        }
    }

    void Evict() {
        this->evict();
    }
    
      // https://stackoverflow.com/a/11398748
      inline int fast_log2_64(uint64_t value) {
            static const int fast_log2_64_lut[64] = {
                63, 0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,  61, 51, 37, 40, 49, 18,
                28, 20, 55, 30, 34, 11, 43, 14, 22, 4,  62, 57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19,
                29, 10, 13, 21, 56, 45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5,
            };

            value |= value >> 1;
            value |= value >> 2;
            value |= value >> 4;
            value |= value >> 8;
            value |= value >> 16;
            value |= value >> 32;
            return fast_log2_64_lut[(static_cast<uint64_t>((value - (value >> 1)) * 0x07EDD5E59A4E28C2) >> 58)];
      }

      inline void first_last_virtual_bucket_on_level(u64 vtree_level, u64 *first, u64 *last) const {
        *first = (vtree_level > 0) ? buckets_on_virtual_level(vtree_level - 1) + 1 : 0;
        *last = *first + ((1UL << (LPP * vtree_level))) - 1;
      }

      inline u64 buckets_on_virtual_level(u64 vtree_level) const {
        return ((1ULL << (LPP * (vtree_level+1))) - 1)/ ((1 << LPP) - 1);
      }

      inline u64 total_large_bucket_node_count(u64 vtree_height) const {
        // return ((1UL << (LPP * (vtree_height + 1))) - 1) / ((1UL << LPP) - 1);
        u64 total = 0;
        for (u64 i = 0; i < vtree_height; i++) {
            total += 1UL << (i * LPP);
        }

        return total;
      }

      /*
        vbu: Enigmap's large bucket
        vbu_offset: Offset of the small bucket within the large bucket
      */
      void bucket_to_vbucket(ORBucketID bu, ORVirtualBucketID *vbu, ORVirtualBucketOffset *vbu_offset) {
        // get the level of the node in the original tree
        u64 node_level = fast_log2_64(bu);
        u64 node_level_first_node_id = (1UL << node_level);
        // get the level of the node in the large bucket tree
        u64 vnode_level = node_level / LPP;

        // get the relative level of the node within the packed subtree it appears in
        u64 node_level_in_packed_subtree = node_level % LPP;

        // get the index of the small bucket at the node level
        u64 small_bucket_level_ix = bu - node_level_first_node_id;

        // determine which large bucket in the level the small bucket is in
        // each subtree level in the packed subtree across all large buckets in that level has 2^i nodes
        u64 packed_small_bucket_stride = 1UL << node_level_in_packed_subtree;
        u64 large_bucket_level_ix = small_bucket_level_ix / packed_small_bucket_stride;

        // get the vnode id of the first large bucket at the large bucket level
        u64 total_large_buckets_above_vnode_level =
            (vnode_level > 0) ? buckets_on_virtual_level(vnode_level - 1) : 0;
        u64 large_bucket_level_first_node_id = 1UL + total_large_buckets_above_vnode_level;
        u64 vnode_id = large_bucket_level_first_node_id + large_bucket_level_ix;

        // determine the node index within the packed subtree
        // get the relative node id of the first node id in the subtree level
        u64 packed_subtree_relative_level_first_node_id = (1UL << node_level_in_packed_subtree);
        u64 packed_subtree_relative_level_ix = small_bucket_level_ix % packed_small_bucket_stride;
        u64 packed_subtree_relative_node_id =
            packed_subtree_relative_level_first_node_id + packed_subtree_relative_level_ix;

        // compute the block offset of the small bucket's blocks within the packed subtree
        u64 packed_bucket_index = packed_subtree_relative_node_id - 1; // blocks are packed starting from 0
        // u64 bucket_start_block_offset = PathORAMClient<B>::BucketSize() * packed_bucket_index;
       u64 bucket_start_block_offset = packed_bucket_index; 

        *vbu = vnode_id;
        *vbu_offset = bucket_start_block_offset;
      }

private:
        TPathORAMLBChannel channel_;
        utils::Key EK;
        u64 buckets_per_page = (1 << LPP) - 1;

        PathORAMLBClient(size_t n, 
                TPathORAMLBChannel channel,
                utils::Key key) : PathORAMClient<B>::PathORAMClient(n, nullptr, key), channel_(std::move(channel)) {
            PathORAMClient<B>::successful = true;
            EK = key;
            large_bucket_size = PathORAMClient<B>::EncryptedBucketSize() * ((1 << LPP) - 1);
            ll_ = std::ceil((log2(n) + 1) / LPP);
        }
     void Setup(std::vector<common::Block<B>> &blocks) {
        // Careful here, cache_ contains ORVirtualBucketID and not ORBucketID
        for (auto b : blocks) {
            Leaf leaf = min_leaf_ + random_gen::generateRandomNumber(n_);
            pos_map_.insert({b.key, leaf});
        }

        u64 tree_height = std::ceil((log2(n_)+1)/LPP); 
        u64 total_large_buckets = total_large_bucket_node_count(tree_height);

        std::cout << "\tVirtual tree height = " << tree_height << std::endl
                    << "\tTotal large buckets = " << total_large_buckets << std::endl;
        for (u64 i = 1; i <= total_large_buckets; i++) {
            cache_.insert(i);
        }

        for (auto b : blocks) {
            stash_.push_back(b);
        }

        PathORAMClient<B>::setup_ = true;
        evict();
      }

      void Read(Keyword w, uint8_t *data) {
        Leaf leaf = pos_map_[w];
        std::vector<ORBucketID> path;
        PathORAMClient<B>::getPathToLeaf(leaf, path);
        bool found = false;

        std::vector<ORVirtualBucketID> vpath;
        std::cout << "Looking for path to leaf " << leaf << std::endl;
        for (auto bu : path) {
            ORVirtualBucketID vbu;
            ORVirtualBucketOffset vbu_offset;
            bucket_to_vbucket(bu + 1, &vbu, &vbu_offset);
            if (std::find(vpath.begin(), vpath.end(), vbu) == vpath.end()) {
                vpath.push_back(vbu);
            }
        }

        read_path(vpath);
        pos_map_[w] = min_leaf_ + random_gen::generateRandomNumber(PathORAMClient<B>::n_);

        for (auto &b : stash_) {
            if (b.key == w) {
                found = true;
                // memcpy(b.val, data, B);
                std::copy(b.val, b.val + B, data);
            }
        }
        
        if (!found) {
            throw std::runtime_error("Looking for keyword " + std::to_string(w) + " failed");
            exit(1);
        }
      }

      void Write(Keyword w, uint8_t *data) {
        Leaf leaf = PathORAMClient<B>::pos_map_[w];
        std::vector<ORBucketID> path;
        PathORAMClient<B>::getPathToLeaf(leaf, path);

        std::vector<ORVirtualBucketID> vpath;
        for (auto bu : path) {
            ORVirtualBucketID vbu;
            ORVirtualBucketOffset vbu_offset;
            bucket_to_vbucket(bu + 1, &vbu, &vbu_offset);
            if (std::find(vpath.begin(), vpath.end(), vbu) == vpath.end()) 
                vpath.push_back(vbu);
        }

        read_path(vpath);

        for (auto b : PathORAMClient<B>::stash_) {
            if (b.key == w) {
                memcpy(b.val, data, B);
            }
        }

        PathORAMClient<B>::pos_map_[w] = PathORAMClient<B>::min_leaf_ + random_gen::generateRandomNumber(PathORAMClient<B>::n_);
      }

    //   char* serializeEncryptBycket_par(std::map<ORVirtualBucketID, std::array<common::Bucket<B>, ((1 << LPP)-1)>>& to_write) {
    //     const size_t bucket_size = PathORAMClient<B>::BucketSize();
    //     const size_t encrypted_bucket_size = PathORAMClient<B>::EncryptedBucketSize();
    //     const size_t large_bucket_size = encrypted_bucket_size * ((1 << LPP)-1);
    //     const size_t total_size = large_bucket_size * to_write.size();
        
    //     // Allocate the final buffer
    //     char* enc_large_buckets = (char*)malloc(sizeof(char) * total_size);
        
    //     // Create thread pool with number of hardware threads
    //     ThreadPool pool(std::thread::hardware_concurrency());
        
    //     // Structure to hold all futures
    //     std::vector<std::future<void>> futures;
        
    //     // Mutex for synchronizing cur_pos updates
    //     std::mutex pos_mutex;
    //     char* cur_pos = enc_large_buckets;

    //     // Structure to pass to worker threads
    //     struct EncryptTask {
    //         common::Bucket<B>* bucket;
    //         char* output_pos;
    //         size_t bucket_size;
    //         const char* EK;  // Assuming EK is your encryption key
    //     };

    //     // Worker function
    //     auto encrypt_bucket = [](void* arg) -> void {
    //         EncryptTask* task = static_cast<EncryptTask*>(arg);
            
    //         // Create temporary buffer for serialization
    //         std::unique_ptr<char[]> temp_buffer(new char[task->bucket_size]);
            
    //         // Serialize
    //         task->bucket->serialize(temp_buffer.get());
            
    //         // Encrypt
    //         if (!utils::Encrypt(temp_buffer.get(), task->bucket_size, task->EK, task->output_pos)) {
    //             throw std::runtime_error("Failed to encrypt bucket");
    //         }
    //     };

    //     // Queue up all encryption tasks
    //     for (auto& entry : to_write) {
    //         auto& [buID, arr_bu] = entry;
            
    //         for (size_t i = 0; i < ((1 << LPP)-1); i++) {
    //             // Get the next position in the output buffer
    //             char* output_pos;
    //             {
    //                 std::lock_guard<std::mutex> lock(pos_mutex);
    //                 output_pos = cur_pos;
    //                 cur_pos += encrypted_bucket_size;
    //             }

    //             // Create task data
    //             auto task_data = new EncryptTask{
    //                 &arr_bu[i],
    //                 output_pos,
    //                 bucket_size,
    //                 EK
    //             };

    //             // Queue the task
    //             futures.push_back(
    //                 pool.enqueue_void_ptr(static_cast<void(*)(void*)>(encrypt_bucket), task_data)
    //             );
    //         }
    //     }

    //     // Wait for all encryptions to complete
    //     for (auto& future : futures) {
    //         try {
    //             future.get();
    //         } catch (const std::exception& e) {
    //             // Clean up allocated memory
    //             free(enc_large_buckets);
    //             throw std::runtime_error(std::string("Encryption failed: ") + e.what());
    //         }
    //     }

    //     return enc_large_buckets;
    // }

      char *
      serializeEncryptBycket(std::map<ORVirtualBucketID,  std::array<common::Bucket<B>, ((1 << LPP)-1)>> &to_write) {
        const size_t bucket_size = PathORAMClient<B>::BucketSize();
        // const size_t large_bucket_size = PathORAMClient<B>::EncryptedBucketSize() * ((1 << LPP)-1);
        char *enc_large_buckets = (char *)malloc(sizeof(char) * large_bucket_size * to_write.size());
        char *cur_pos = enc_large_buckets;

        for (auto &entry : to_write) {
            auto &[buID, arr_bu] = entry;
            for (size_t i = 0; i < ((1 << LPP)-1); i++) {
                std::unique_ptr<char[]> temp_buffer(new char[bucket_size]);
                arr_bu[i].serialize(temp_buffer.get());

                if (!utils::Encrypt(temp_buffer.get(), bucket_size, EK, cur_pos)) {
                    throw std::runtime_error("Failed to encrypt bucket [LINE: " + std::to_string(__LINE__) + "]");
                    exit(1);
                }

                cur_pos += PathORAMClient<B>::EncryptedBucketSize();
            }
        }

        return enc_large_buckets;
      }

      std::map<ORVirtualBucketID, char *>
      encryptBuckets(std::map<ORVirtualBucketID, std::array<common::Bucket<B>, ((1 << LPP)-1)>> &to_write) {
        std::map<ORVirtualBucketID, char *> enc_large_buckets;
        const size_t bucket_size = PathORAMClient<B>::BucketSize();
        // const size_t large_bucket_size = PathORAMClient<B>::EncryptedBucketSize() * ((1 << LPP) - 1);


        for ( auto& [buID, arr_bu] : to_write) {
            // std::cout << "Encrypting large bucket [{" << buID << "}]\n"
            //     << "\twith " << arr_bu.size() << " small buckets\n"
            //     << "\tbucket size: " << bucket_size << "\n"
            //     << "\tlarge bucket size: " << large_bucket_size
            //     << std::endl;

            if (arr_bu.size() != buckets_per_page) {
                throw std::runtime_error("Invalid bucket size");
                exit(1);
            }

            auto large_bucket = std::make_unique<char[]>(PathORAMLBClient<B>::EncryptedLargeBucketSize() * sizeof(char));
            char *cur_pos = large_bucket.get();

            for (size_t i = 0; i < ((1 << LPP) - 1); i++) {
                std::unique_ptr<char[]> temp_buffer(new char[bucket_size]);
                arr_bu[i].serialize(temp_buffer.get());

                if (!utils::Encrypt(temp_buffer.get(), bucket_size, EK, cur_pos)) {
                    throw std::runtime_error("Failed to encrypt bucket [LINE: " + std::to_string(__LINE__) + "]");
                    exit(1);
                }

                cur_pos += PathORAMClient<B>::EncryptedBucketSize();
            }

            enc_large_buckets[buID] = (char *)malloc(sizeof(char) * PathORAMLBClient<B>::EncryptedLargeBucketSize());
            std::memcpy(enc_large_buckets[buID], large_bucket.get(), PathORAMLBClient<B>::EncryptedLargeBucketSize());
            large_bucket.release();
        }

        return enc_large_buckets;
      }

      void evict() {
        if (PathORAMClient<B>::stash_.empty()) { return; }

        std::map<ORVirtualBucketID, std::array<common::Bucket<B>, ((1 << LPP)-1)>> to_write;
        
        for (auto level = ll_-1; level >= 0; level--) {
            // Computing the cached nodes in the current level
            u64 min_level, max_level;
            size_t flags;
            first_last_virtual_bucket_on_level(level, &min_level, &max_level);
            min_level = (level > 0) ? min_level : 1;
            max_level = (level > 0) ? max_level : 1;
            auto low = cache_.lower_bound(min_level);
            auto high = cache_.lower_bound(max_level);
            // std::cout << "Evicting level " << level << std::endl
            //         << "LPP: " << LPP << std::endl
            //         << "Low: " << *low << std::endl
            //         << "High: " << *high << std::endl;

 
            for (ORVirtualBucketID id = *low; id <= *high; id++) {
                to_write[id] = {};
            }

            // Now, we need to place the blocks in the stash into the buckets
            // they can reside into.
            for (size_t block_idx = 0; block_idx < PathORAMClient<B>::stash_.size(); block_idx++) {
                auto b = PathORAMClient<B>::stash_[block_idx];
                std::vector<ORBucketID> path;
                auto leaf = PathORAMClient<B>::pos_map_[b.key];
                assert(leaf >= PathORAMClient<B>::min_leaf_ && leaf <=PathORAMClient<B>::max_leaf_);
                PathORAMClient<B>::getPathToLeaf(leaf, path);
                common::Bucket<B> *bucket = nullptr;

                assert(path.size() == PathORAMClient<B>::l_ + 1);

                ORVirtualBucketID vbuID;
                ORVirtualBucketOffset vbu_offset;
                for (auto base_bu : path) {
                    u64 base_bu_level = fast_log2_64(base_bu);
                    if ( (base_bu_level/LPP) != level && base_bu_level != 0) {
                        continue;
                    }
                
                    bucket_to_vbucket(base_bu + 1, &vbuID, &vbu_offset);

                    if (to_write.find(vbuID) == to_write.end()) {
                        continue;
                    }

                    bucket = &to_write[vbuID][vbu_offset];
                    // If the bucket is full, continue
                    flags = bucket->flags_;
                    if (flags == Z) {
                        continue;
                    }
                }
                
                // Overflown bucket, we need to keep in stash and evict later.
                if (flags == Z || bucket == nullptr) {
                    continue;
                }
                // Add the block to the bucket 
                bucket->blocks_[flags].key = b.key;
                std::memcpy(bucket->blocks_[flags].val, b.val, B);
                bucket->flags_++;
                PathORAMClient<B>::stash_.erase(PathORAMClient<B>::stash_.begin() + block_idx);

            }
            if (level == 0) { break; }
        }
        
        if (PathORAMClient<B>::stash_.size() > PathORAMClient<B>::max_stash_size_ && PathORAMClient<B>::setup_ == false) {
            throw std::runtime_error("Stash size exceeded");
            exit(1);
        }

        std::map<ORVirtualBucketID, char *> large_buckets = encryptBuckets(to_write);

        channel_->write_buckets(large_buckets);
        large_buckets.clear();
        PathORAMClient<B>::cache_.clear();
        to_write.clear();
      }

      void read_path(std::vector<ORBucketID> &vids) {
        std::vector<char *> enc_large_buckets;
        size_t en_bus_ = PathORAMClient<B>::EncryptedBucketSize();
        for (auto id : vids) {
          char *en_bu = (char *)malloc(PathORAMLBClient<B>::EncryptedLargeBucketSize() * sizeof(char));
          enc_large_buckets.push_back(en_bu);
        }

        channel_->read_buckets(vids, enc_large_buckets);

        for (auto en_lbu : enc_large_buckets) {
            assert(en_lbu);
            size_t offset = 0;

            assert(buckets_per_page == (1 << LPP)-1);
            for (size_t i = 0; i < buckets_per_page; i++) {
                // std::cout << "buckets_per_page: " << buckets_per_page 
                //     << ", en_bus_: " << en_bus_
                //     << ", offset: " << offset
                //     << std::endl;
                auto vbu_ser = std::make_unique<char[]>(en_bus_);

                auto dec = utils::Decrypt(en_lbu + offset, PathORAMClient<B>::EncryptedBucketSize(), EK, vbu_ser.get());
                if (dec != PathORAMClient<B>::BucketSize()) {
                    std::cerr << "Bucket size mismatch: dec=" << dec 
                        << ", Expected=" << PathORAMClient<B>::EncryptedBucketSize() << std::endl;
                    throw std::runtime_error("Failed to decrypt bucket [LINE: " + std::to_string(__LINE__) + "]");
                    exit(1);
                }
                common::Bucket<B> bu;
                bu.deserialize(vbu_ser.get());

                for (char blocks = 0; blocks < bu.flags_; blocks++) {
                    PathORAMClient<B>::stash_.push_back(bu.blocks_[blocks]);
                }

                offset += en_bus_;
            }
        }

        enc_large_buckets.clear();
      }  
};
