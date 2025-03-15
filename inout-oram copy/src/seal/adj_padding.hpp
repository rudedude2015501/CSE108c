// Implementation of ADJ-PADDING as described in the paper
template<size_t B>
class ADJPadding {
public:
    // Pad the dataset to reduce volume pattern leakage
    static std::vector<std::pair<std::string, std::vector<uint32_t>>> PadDataset(
        const std::vector<std::pair<std::string, std::vector<uint32_t>>>& dataset, 
        size_t x) {
        
        std::vector<std::pair<std::string, std::vector<uint32_t>>> padded_dataset = dataset;
        size_t N = 0;
        
        // Count total number of entries
        for (const auto& [keyword, doc_ids] : dataset) {
            N += doc_ids.size();
        }
        
        // For each keyword, pad to the closest power of x
        for (auto& [keyword, doc_ids] : padded_dataset) {
            size_t size = doc_ids.size();
            size_t i = 0;
            
            // Find smallest i such that x^i >= size
            size_t target_size = 1;
            while (target_size < size) {
                target_size *= x;
                i++;
            }
            
            // Pad with dummy values to x^i
            size_t pad_amount = target_size - size;
            for (size_t j = 0; j < pad_amount; j++) {
                // Use a special marker for dummy documents (e.g., using high bits)
                doc_ids.push_back(0xFFFFFFFF - j);  // Dummy document IDs
            }
        }
        
        // Add dummy records so that the total size is x * N
        size_t total_size_after_padding = 0;
        for (const auto& [keyword, doc_ids] : padded_dataset) {
            total_size_after_padding += doc_ids.size();
        }
        
        size_t final_target_size = x * N;
        if (total_size_after_padding < final_target_size) {
            // Add a dummy keyword with dummy entries
            std::vector<uint32_t> dummy_docs;
            for (size_t i = 0; i < (final_target_size - total_size_after_padding); i++) {
                dummy_docs.push_back(0xFFFFFFF0 - i);
            }
            padded_dataset.push_back({"__DUMMY_KEYWORD__", dummy_docs});
        }
        
        return padded_dataset;
    }
    
    // Helper method to calculate the next power of x greater than or equal to n
    static size_t NextPowerOf(size_t x, size_t n) {
        size_t power = 1;
        while (power < n) {
            power *= x;
        }
        return power;
    }
    
    // Get the number of different possible padded sizes
    static size_t DistinctSizes(size_t N, size_t x) {
        // For a dataset of size N, the number of distinct sizes is log_x(N) + 1
        if (N == 0) return 1;
        
        size_t count = 0;
        size_t power = 1;
        while (power <= N) {
            power *= x;
            count++;
        }
        
        return count;
    }
    
    // Calculate the maximum storage overhead due to padding
    static double MaxOverhead(size_t x) {
        // The worst case is when all lists are just above a power of x
        // In that case, we pad almost to the next power, which is at most x times more
        return x - 1.0;
    }
    
    // Calculate the average storage overhead due to padding
    static double AverageOverhead(size_t x) {
        // On average, the storage overhead is (x+1)/2 - 1
        return (x + 1.0) / 2.0 - 1.0;
    }
};