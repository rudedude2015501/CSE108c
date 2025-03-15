#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include "seal/seal.hpp"
#include "stopwatch.hpp"

int main() {
    const size_t B = 8;  // Declare B only once
    const size_t alpha = 3;
    const size_t x = 4;
    
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting SEAL test with alpha={}, x={}", alpha, x);
    
    // Test ADJORAM directly if needed
    std::vector<seal::SEAL<B>::KeywordDocPair> testData = {
    seal::SEAL<B>::KeywordDocPair("test", 1),
    seal::SEAL<B>::KeywordDocPair("test", 2),
    seal::SEAL<B>::KeywordDocPair("test", 3)
        };

        auto [state, memory] = seal::ADJORAM<B>::Initialize(128, testData, 2);
        spdlog::info("ADJORAM initialized directly");
    auto [state2, memory2] = seal::ADJORAM<B>::Initialize(128, testData, 2);
    spdlog::set_level(spdlog::level::debug); 
    spdlog::info("Starting SEAL test with alpha={}, x={}", alpha, x);
    
    // Create a sample dataset
    std::vector<std::pair<std::string, std::vector<uint32_t>>> dataset = {
        {"apple", {1, 3, 5, 7, 9}},
        {"banana", {2, 4, 6, 8, 10, 12, 14}},
        {"cherry", {1, 5, 9, 13, 17}},
        {"date", {3, 4, 12, 18, 19, 20}}
    };
    
    // Log dataset details
    spdlog::info("Dataset contains {} keywords:", dataset.size());
    for (const auto& [keyword, docs] : dataset) {
        spdlog::info("  '{}': {} documents", keyword, docs.size());
    }
    
    // Sequential setup
    spdlog::info("[SEQ-SETUP] SEAL initialization with {} keywords", dataset.size());
    
    Stopwatch sw;
    sw.start();
    
    auto [client_state, server_index] = seal::SEAL<B>::Setup(128, dataset, alpha, x);
    
    spdlog::info("[SEQ-SETUP] time elapsed for SEAL setup: {:.6f} seconds", sw.elapsed_sec());
    
    // Perform searches
    std::vector<std::string> queries = {"apple", "banana", "cherry", "date", "fig"};
    
    for (const auto& query : queries) {
        spdlog::info("Searching for keyword: '{}'", query);
        
        sw.start();
        
        auto [results, new_client_state] = seal::SEAL<B>::Search(
            client_state, server_index, query, alpha);
        
        client_state = new_client_state;
        
        spdlog::info("Found {} results in {:.6f} seconds", results.size(), sw.elapsed_sec());
        
        // Add eviction information similar to PathORAM
        spdlog::info("Evicting after search for '{}'", query);
        
        // Display a sample of results (if any)
        if (!results.empty()) {
            spdlog::info("Sample document IDs: {}, {}, ...", 
                results[0], 
                results.size() > 1 ? results[1] : results[0]);
        }
    }
    
    // Test different parameter configurations
    spdlog::info("\nTesting SEAL with different configurations:");
    
    // Test different alpha values
    for (size_t test_alpha : {0, 2, 4, 6}) {
        if (test_alpha == alpha) continue; // Skip the one we already tested
        
        spdlog::info("Setting alpha={} (access pattern bits leaked)", test_alpha);
        sw.start();
        
        auto [new_client_state, new_server_index] = seal::SEAL<B>::Setup(128, dataset, test_alpha, x);
        
        spdlog::info("Setup completed in {:.6f} seconds", sw.elapsed_sec());
        
        // Test one search
        sw.start();
        auto [results, updated_state] = seal::SEAL<B>::Search(
            new_client_state, new_server_index, "apple", test_alpha);
        
        spdlog::info("Search completed in {:.6f} seconds, found {} results", 
            sw.elapsed_sec(), results.size());
    }
    
    return 0;
}