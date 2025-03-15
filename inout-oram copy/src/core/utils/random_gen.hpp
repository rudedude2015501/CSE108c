#pragma once
#include <random>

namespace random_gen {
 
int generateRandomNumber(int n) {
    if (n <= 0) {
        throw std::invalid_argument("n must be greater than 0");
    }

    // Create a random device and seed the generator
    std::random_device rd; 
    std::mt19937 gen(rd());

    // Define the range [0, n-1]
    std::uniform_int_distribution<> dist(0, n - 1);

    // Generate and return the random number
    return dist(gen);
}

template <size_t B>
std::array<uint8_t,B> GenRandBytes() {
    std::array<uint8_t,B> res{};
    for (size_t i = 0; i < B; i++) {
        res[i] = generateRandomNumber(255);
    }
    return res;
}

} // namespace random_gen