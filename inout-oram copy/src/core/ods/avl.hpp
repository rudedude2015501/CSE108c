#include "oram/path_oram/path_oram.hpp"
#include "oram/common/block.hpp"

using AVLKey = uint32_t;

struct AVLPointer {
    ORKey orkey_;
    Leaf pos_;
    bool valid_ = false;
};

template <size_t B>
struct AVLBlock {
    AVLKey key_;
    uint8_t val_[B];

    //metadata
    ORBlockID l_, r_;
    uint8_t height_;
    Leaf pos_l_, pos_r_;

    void serialize(char *buf) {
        char *ptr = buf;
        memcpy(ptr, &key_, sizeof(AVLKey));
        ptr += sizeof(AVLKey);
        
        memcpy(ptr, val_, B);
        ptr += B;
        
        memcpy(ptr, &l_, sizeof(ORKey));
        ptr += sizeof(ORKey);
        
        memcpy(ptr, &r_, sizeof(ORKey));
        ptr += sizeof(ORKey);
        
        *ptr++ = height_;
        
        memcpy(ptr, &pos_l_, sizeof(Leaf));
        ptr += sizeof(Leaf);
        
        memcpy(ptr, &pos_r_, sizeof(Leaf));
    }

    void deserialize(char *buf) {
        char *ptr = buf;
        memcpy(&key_, ptr, sizeof(AVLKey));
        ptr += sizeof(AVLKey);
        
        memcpy(val_, ptr, B);
        ptr += B;
        
        memcpy(&l_, ptr, sizeof(ORKey));
        ptr += sizeof(ORKey);
        
        memcpy(&r_, ptr, sizeof(ORKey));
        ptr += sizeof(ORKey);
        
        height_ = *ptr++;
        
        memcpy(&pos_l_, ptr, sizeof(Leaf));
        ptr += sizeof(Leaf);
        
        memcpy(&pos_r_, ptr, sizeof(Leaf));
    }
};

template <size_t AVLData>
class OMap {
public:
    OMap() = default;
    ~OMap() = default;
    inline static constexpr size_t AVLBlockSize() { return sizeof(AVLKey) + 2*sizeof(Leaf) + sizeof(uint8_t) + 2*sizeof(ORKey) + B; }

    void Insert(AVLKey key, char *value) {
        // Insert key-value pair into the AVL tree
    }

    int Read(AVLKey key) {
        this->Read(key, root_);
    }
private:
    AVLPointer *root_;
    PathORAMClient<AVLBlockSize()> *oram_;

    void Read(AVLKey key, AVLPointer *node) {
        if (node == nullptr) {
            return;
        }

        auto &b = oram_->FetchPath()
        if (key < node->key_) {
            Read(key, node->l_);
        } else if (key > node->key_) {
            Read(key, node->r_);
        } else {
            // Key found, return the value
            return node->val_;
        }
    }
}