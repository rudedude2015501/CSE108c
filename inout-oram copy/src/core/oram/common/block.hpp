#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>

using ORBucketID = uint32_t;
using Leaf = uint32_t;

namespace common {
    #define Z 4
    // Represents a simple ORAM block
    template <size_t B>
    struct Block {
        uint32_t key;
        uint8_t val[B];

        Block() : key(-1) {
            memset(val, 0, B);
        }

        Block (uint32_t key, const uint8_t* val) : key(key) {
            memcpy(this->val, val, B);
        }

        Block(uint32_t key, const std::array<uint8_t, B>& val) : key(key) {
            memcpy(this->val, val.data(), B);
        }

        Block (const Block& other) : key(other.key) {
            memcpy(val, other.val, B);
        }

        Block(Block&& other) noexcept : key(other.key) {
            memcpy(val, other.val, B);
            other.key = 0;
            memset(other.val, 0, B); // Optional, for safety
        }

        Block& operator=(Block&& other) noexcept {
            if (this != &other) {
                key = other.key;
                memcpy(val, other.val, B);
                other.key = 0;
                memset(other.val, 0, B); // Optional, for safety
            }
            return *this;
        }

        void serialize(char *buf) {
            if(!buf) {
                buf = new char[sizeof(uint32_t) + B];
            } 

            memcpy(buf, &key, sizeof(uint32_t));
            memcpy(buf + sizeof(uint32_t), val, B);
        }

        void deserialize(const char *buf) {
            memcpy(&key, buf, sizeof(uint32_t));
            memcpy(val, buf + sizeof(uint32_t), B);
        }
    };

    // Represents a simple ORAM bucket
    template <size_t B>
    struct Bucket {
       char flags_;
       std::vector<Block<B>> blocks_;

       Bucket() : flags_(0) {
            for (size_t i = 0; i < Z; i++) {
                blocks_.push_back(Block<B>());
            }
       }

       void serialize(char *buf) {
            if (!buf) {
                throw std::invalid_argument("Buffer cannot be null");
            }
            
            memcpy(buf, &flags_, sizeof(char));
            char *ptr = buf + sizeof(char);
            
            for (size_t i = 0; i < Z; i++) {
                blocks_[i].serialize(ptr);
                ptr += sizeof(uint32_t) + B;
            }
        }

        void deserialize(const char *buf) {
            if (!buf) {
                throw std::invalid_argument("Buffer cannot be null");
            }
            
            memcpy(&flags_, buf, sizeof(char));
            const char *ptr = buf + sizeof(char);
            
            for (size_t i = 0; i < Z; i++) {
                blocks_[i].deserialize(ptr);
                
                ptr += sizeof(uint32_t) + B;
            }
        }
   };
} // namespace common
