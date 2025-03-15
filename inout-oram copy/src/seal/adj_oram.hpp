// Implementation of ADJ-ORAM (Adjustable Oblivious RAM)
template<size_t B>
class ADJORAM {
public:
    struct State {
        std::vector<uint8_t> key;
        size_t alpha;
        size_t N;
        std::vector<PathORAMClient<B>*> orams;
    };
    
    struct EncryptedMemory {
        std::vector<std::shared_ptr<channel::PathORAMChannel<char*, PathORAMClient<B>::EncryptedBucketSize()>>> channels;
    };
    
    // Convert a KeywordDocPair to a Block for storage in ORAM
    static common::Block<B> KeywordDocPairToBlock(const typename SEAL<B>::KeywordDocPair& pair) {
        spdlog::debug("Converting KeywordDocPair to Block: keyword={}, doc_id={}", pair.keyword, pair.doc_id);
        common::Block<B> block;
        block.key = pair.doc_id;
        
        // Copy keyword data to block value (limited by B)
        std::memset(block.val, 0, B);
        size_t copy_size = std::min(pair.keyword.size(), (size_t)B);
        std::memcpy(block.val, pair.keyword.c_str(), copy_size);
        
        return block;
    }
    
    // Convert a Block back to KeywordDocPair
    static typename SEAL<B>::KeywordDocPair BlockToKeywordDocPair(const common::Block<B>& block) {
        typename SEAL<B>::KeywordDocPair pair;
        pair.doc_id = block.key;
        
        // Extract keyword from block value (null-terminated)
        pair.keyword = std::string(reinterpret_cast<const char*>(block.val), strnlen(reinterpret_cast<const char*>(block.val), B));
        spdlog::debug("Converting Block to KeywordDocPair: keyword={}, doc_id={}", pair.keyword, pair.doc_id);
        
        return pair;
    }
    
    // Initialize the ADJ-ORAM
    static std::pair<std::shared_ptr<State>, std::shared_ptr<EncryptedMemory>> Initialize(
        size_t security_param,
        const std::vector<typename SEAL<B>::KeywordDocPair>& memory,
        size_t alpha) {
        
        spdlog::info("===== REAL ADJ-ORAM Initialize Start =====");
        spdlog::info("Memory size: {}, Alpha: {}", memory.size(), alpha);
        
        auto state = std::make_shared<State>();
        auto encrypted_memory = std::make_shared<EncryptedMemory>();
        
        // Generate random key
        state->key = CreateRandomKey();
        spdlog::debug("Generated random key of size: {}", state->key.size());
        
        state->alpha = alpha;
        state->N = memory.size();
        
        // Number of regions = 2^alpha
        size_t num_regions = 1 << alpha;
        
        spdlog::info("ADJ-ORAM using {} regions (2^{})", num_regions, alpha);
        
        // Create channels and ORAMs for each region
        encrypted_memory->channels.resize(num_regions);
        state->orams.resize(num_regions);
        
        // Partition data into regions based on alpha most significant bits
        std::vector<std::vector<typename SEAL<B>::KeywordDocPair>> regions(num_regions);
        
        // Create pseudorandom permutation function
        auto prp = [&state](uint32_t i) -> uint32_t {
            // Simple PRP simulation using the key
            std::hash<std::string> hasher;
            std::string to_hash(reinterpret_cast<char*>(&i), sizeof(i));
            to_hash.append(reinterpret_cast<const char*>(state->key.data()), state->key.size());
            return hasher(to_hash);
        };
        
        // Assign each item to a region based on alpha MSBs of PRP
        spdlog::info("Partitioning {} items into {} regions", memory.size(), num_regions);
        for (const auto& pair : memory) {
            uint32_t permuted_idx = prp(pair.doc_id);
            size_t region_idx = permuted_idx >> (32 - alpha); // Use alpha most significant bits
            regions[region_idx].push_back(pair);
            spdlog::debug("Item (keyword={}, doc_id={}) assigned to region {}", 
                         pair.keyword, pair.doc_id, region_idx);
        }
        
        // Log region sizes
        for (size_t i = 0; i < num_regions; i++) {
            spdlog::info("Region {} contains {} items", i, regions[i].size());
        }
        
        // Initialize each region with its own ORAM
        for (size_t i = 0; i < num_regions; i++) {
            // Setup server config
            server::ServerConfig config;
            config.type = server::ServerConfig::StorageType::Memory;
            
            spdlog::info("Creating PathORAMChannel for region {}", i);
            // Create channel
            encrypted_memory->channels[i] = std::make_shared<channel::PathORAMChannel
                char*, PathORAMClient<B>::EncryptedBucketSize()>>(config);
            
            // Calculate region capacity (ensure at least 1)
            size_t region_capacity = std::max(size_t(1), regions[i].size());
            
            spdlog::info("REAL PathORAM initialization for region {} with capacity {}", i, region_capacity);
            
            // Initialize ORAM for this region
            std::optional<PathORAMClient<B>*> opt_oram = PathORAMClient<B>::Construct(
                region_capacity, encrypted_memory->channels[i], state->key);
            
            if (!opt_oram.has_value()) {
                spdlog::error("Failed to initialize ORAM for region {}", i);
                throw std::runtime_error("Failed to initialize ORAM for region " + std::to_string(i));
            }
            
            spdlog::info("PathORAMClient successfully constructed for region {}", i);
            state->orams[i] = opt_oram.value();
            
            // Convert KeywordDocPair to common::Block for ORAM initialization
            std::vector<common::Block<B>> blocks;
            for (size_t j = 0; j < regions[i].size(); j++) {
                blocks.push_back(KeywordDocPairToBlock(regions[i][j]));
            }
            
            // If the region is empty, add a dummy block
            if (blocks.empty()) {
                spdlog::info("Adding dummy block to empty region {}", i);
                blocks.push_back(common::Block<B>());
            }
            
            spdlog::info("Initializing ORAM {} with {} blocks", i, blocks.size());
            // Initialize ORAM with blocks
            state->orams[i]->Init(blocks);
            spdlog::info("ORAM {} initialized successfully", i);
        }
        
        spdlog::info("===== REAL ADJ-ORAM Initialize Complete =====");
        return {state, encrypted_memory};
    }
    
    // Perform an ORAM access
    static std::pair<typename SEAL<B>::KeywordDocPair, std::shared_ptr<State>> Access(
        const std::shared_ptr<State>& state,
        std::shared_ptr<EncryptedMemory>& encrypted_memory,
        const std::string& op, // "read" or "write"
        size_t index,
        const typename SEAL<B>::KeywordDocPair& value,
        size_t alpha) {
        
        spdlog::info("===== REAL ADJ-ORAM Access Start =====");
        spdlog::info("Operation: {}, Index: {}, Alpha: {}", op, index, alpha);
        
        // Create PRP function (same as in Initialize)
        auto prp = [&state](uint32_t i) -> uint32_t {
            std::hash<std::string> hasher;
            std::string to_hash(reinterpret_cast<char*>(&i), sizeof(i));
            to_hash.append(reinterpret_cast<const char*>(state->key.data()), state->key.size());
            return hasher(to_hash);
        };
        
        // Calculate which region to access and position within the region
        uint32_t permuted_idx = prp(index);
        size_t region_idx = permuted_idx >> (32 - alpha); // Use alpha most significant bits
        size_t pos_in_region = permuted_idx & ((1 << (32 - alpha)) - 1); // Use remaining bits
        
        spdlog::info("REAL ORAM access: region {}, position {} (original index {})", 
                    region_idx, pos_in_region, index);
        
        // Check if region index is valid
        if (region_idx >= state->orams.size()) {
            spdlog::error("Invalid region index: {} (max: {})", region_idx, state->orams.size() - 1);
            throw std::runtime_error("Invalid region index");
        }
        
        // Check if ORAM exists for this region
        if (!state->orams[region_idx]) {
            spdlog::error("Null ORAM for region {}", region_idx);
            throw std::runtime_error("Null ORAM for region");
        }
        
        // Perform the actual ORAM access
        typename SEAL<B>::KeywordDocPair result;
        
        if (op == "read") {
            spdlog::info("REAL PathORAM Read operation on region {}, position {}", region_idx, pos_in_region);
            // Read from the ORAM
            common::Block<B> block;
            state->orams[region_idx]->Read(pos_in_region, block);
            spdlog::info("REAL PathORAM Read completed with block key: {}", block.key);
            
            // Convert block back to KeywordDocPair
            result = BlockToKeywordDocPair(block);
            
            // Perform eviction
            spdlog::info("REAL PathORAM Evict operation on region {}", region_idx);
            state->orams[region_idx]->Evict();
            spdlog::info("REAL PathORAM Evict completed for region {}", region_idx);
            
        } else if (op == "write") {
            spdlog::info("REAL PathORAM Write operation on region {}, position {}", region_idx, pos_in_region);
            // Convert KeywordDocPair to Block
            common::Block<B> block = KeywordDocPairToBlock(value);
            
            // Write to the ORAM
            state->orams[region_idx]->Write(pos_in_region, block);
            spdlog::info("REAL PathORAM Write completed with block key: {}", block.key);
            
            // Perform eviction
            spdlog::info("REAL PathORAM Evict operation on region {}", region_idx);
            state->orams[region_idx]->Evict();
            spdlog::info("REAL PathORAM Evict completed for region {}", region_idx);
        }
        
        spdlog::info("===== REAL ADJ-ORAM Access Complete =====");
        return {result, state};
    }
};