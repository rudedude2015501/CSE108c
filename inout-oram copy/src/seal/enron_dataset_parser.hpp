#pragma once
#include <cstdint>
#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <unordered_set>
#include "oram/common/block.hpp"

namespace fs = std::filesystem;

// Common English stopwords to filter out
const std::unordered_set<std::string> STOPWORDS = {
    "the", "and", "to", "of", "in", "a", "is", "that", "for", "this", "you", "it", 
    "with", "are", "be", "on", "not", "have", "was", "but", "as", "at", "from", 
    "by", "an", "they", "we", "their", "or", "will", "if", "can", "all", "has", 
    "who", "what", "when", "where", "why", "how", "would", "could", "should", "did",
    "do", "does", "been", "being", "had", "has", "have", "my", "your", "his", "her",
    "its", "our", "their", "i", "me", "he", "she", "him", "us", "them"
};

// Function to clean text and extract keywords
std::set<std::string> extractKeywords(const std::string& text) {
    std::set<std::string> keywords;
    std::string word;
    std::istringstream iss(text);
    
    while (iss >> word) {
        // Convert to lowercase
        std::transform(word.begin(), word.end(), word.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        
        // Remove non-alphanumeric characters
        word.erase(std::remove_if(word.begin(), word.end(), 
                                  [](unsigned char c){ return !std::isalnum(c); }), 
                   word.end());
        
        // Filter out short words and stopwords
        if (word.length() > 3 && STOPWORDS.find(word) == STOPWORDS.end()) {
            keywords.insert(word);
        }
    }
    
    return keywords;
}

// Convert CSV line to key-value pair using your existing parsing approach
std::pair<std::string, std::vector<uint8_t>> parseCsvLine(const std::string& line) {
    size_t ind = line.find(',');
    if (ind == std::string::npos) {
        return {"", {}};
    }
    
    std::string key = line.substr(0, ind);
    std::string content = line.substr(ind + 1);
    std::vector<uint8_t> data(content.begin(), content.end());
    
    return {key, data};
}

// Process the Enron CSV dataset from Kaggle
std::vector<std::pair<std::string, std::vector<uint32_t>>> parseEnronCSV(
    const std::string& filepath, 
    int max_emails = 10000, 
    int max_keywords = 100,
    bool verbose = true) {
    
    if (verbose) {
        std::cout << "Parsing Enron dataset from: " << filepath << std::endl;
        std::cout << "Max emails: " << max_emails << ", Max keywords: " << max_keywords << std::endl;
    }
    
    // Open the CSV file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        return {};
    }
    
    // Skip header line
    std::string line;
    std::getline(file, line);
    
    // Map to store keyword -> document IDs
    std::map<std::string, std::set<uint32_t>> keyword_to_docs;
    uint32_t doc_id = 0;
    int email_count = 0;
    int total_keywords = 0;
    
    // Process each line (email)
    while (std::getline(file, line) && email_count < max_emails) {
        // Parse the line
        size_t first_comma = line.find(',');
        if (first_comma == std::string::npos) continue;
        
        // Extract the message content (combining subject and body)
        // Format is usually: file,message,date,from,to,etc.
        size_t second_comma = line.find(',', first_comma + 1);
        if (second_comma == std::string::npos) continue;
        
        std::string message = line.substr(first_comma + 1, second_comma - first_comma - 1);
        
        // Extract keywords from the message
        std::set<std::string> keywords = extractKeywords(message);
        
        // Add document to keyword index
        for (const auto& keyword : keywords) {
            keyword_to_docs[keyword].insert(doc_id);
            total_keywords++;
        }
        
        doc_id++;
        email_count++;
        
        if (verbose && email_count % 1000 == 0) {
            std::cout << "Processed " << email_count << " emails..." << std::endl;
        }
    }
    
    if (verbose) {
        std::cout << "Extracted " << keyword_to_docs.size() << " unique keywords from " 
                  << email_count << " emails" << std::endl;
        std::cout << "Total keyword-document pairs: " << total_keywords << std::endl;
    }
    
    // Sort keywords by document frequency (most common first)
    std::vector<std::pair<std::string, size_t>> keyword_counts;
    for (const auto& [keyword, docs] : keyword_to_docs) {
        keyword_counts.push_back({keyword, docs.size()});
    }
    
    std::sort(keyword_counts.begin(), keyword_counts.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Take top max_keywords
    std::vector<std::pair<std::string, std::vector<uint32_t>>> dataset;
    int count = 0;
    
    for (const auto& [keyword, size] : keyword_counts) {
        if (count >= max_keywords) break;
        
        // Skip keywords that appear in too few documents (optional)
        if (size < 3) continue;
        
        std::vector<uint32_t> doc_ids(keyword_to_docs[keyword].begin(), keyword_to_docs[keyword].end());
        dataset.push_back({keyword, doc_ids});
        count++;
        
        if (verbose) {
            std::cout << "Selected keyword: " << keyword << " (appears in " 
                      << doc_ids.size() << " documents)" << std::endl;
        }
    }
    
    return dataset;
}

// Simplified version to directly use with your existing get_blocks function
std::vector<std::pair<std::string, std::vector<uint8_t>>> getEnronBlocks(
    const std::string& filepath, 
    uint32_t block_number) {
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        return {};
    }
    
    // Skip header
    std::string line;
    std::getline(file, line);
    
    std::vector<std::pair<std::string, std::vector<uint8_t>>> output;
    uint32_t count = 0;
    
    while (std::getline(file, line) && count < block_number) {
        size_t ind = line.find(',');
        if (ind == std::string::npos) continue;
        
        std::string key = line.substr(0, ind);
        // Get the message part which is the second field
        size_t next_ind = line.find(',', ind + 1);
        if (next_ind == std::string::npos) continue;
        
        std::string message = line.substr(ind + 1, next_ind - ind - 1);
        // Truncate to fit in your data structure if needed
        if (message.length() > 255) {
            message = message.substr(0, 255);
        }
        
        std::vector<uint8_t> data(message.begin(), message.end());
        output.push_back({key, data});
        count++;
    }
    
    return output;
}

// Function to prepare SEAL-compatible dataset from Enron CSV
std::vector<std::pair<std::string, std::vector<uint32_t>>> prepareEnronDatasetForSEAL(
    const std::string& filepath,
    int max_emails = 5000,
    int max_keywords = 50) {
    
    return parseEnronCSV(filepath, max_emails, max_keywords);
}