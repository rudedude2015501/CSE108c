#include "oram/path_oram/path_oram.hpp"
#include "oram/common/block.hpp"

using AVLKey = uint64_t;
using ORBucketID = uint64_t;

struct BlockPointer{
    Leaf pos_;
    ORBucketID key_;
    bool valid_ = false;
};
using BP = BlockPointer;

struct OSMBMeta {
    AVLKey key_;
    BP l_, r_;
    uint8_t height;
    ORBucketID lc_ = 0, rc_ = 0;
};

class OSMBlock {
public:
    OSMBMeta meta_;
    uint8_t val[B];
}

template <size_t B>
class OSM {
public:
    void Insert(AVLKey key, uint8_t *data) {

    };
    void Init() { this->Setup();};
    void Read(AVLKey key) {

    }
    std::optional<OSM> Construct(size_t n, utils::Key key, TPathORAMChannel channel) {
        auto osm = OSM(n, key, channel);
        if (!o.successful) {
            spdlog::error("Failed to initialize OSM with of n = {} and B = {}", n, B);
            return std::nullopt;
        }
        return osm;
    }

private:
    size_t n_;
    utils::Key EK;
    bool successful = false;
    BP root_; 
    std::map<ORBucketID, OSMBlock> blocks_;

    OSM(size_t n, utils::Key key, TPathORAMChannel channel) : n_(n), EK(key) {
        // Initialize the ORAM
        auto o = new PathORAMClient<B>(n, std::move(channel), key);
        if (!o.has_value()) {
            spdlog::error("Failed to initialize OSM with of n = {} and B = {}", n, B);
        }

        successful = true;
    }


}