#pragma once
#include <cstdint>
#include <optional>

class ChunkedBitset {
  static constexpr int BITS_PER_CHUNK = 64;
  static constexpr int MAX_BITS = 4096;
  static constexpr int NUM_CHUNKS = MAX_BITS / BITS_PER_CHUNK;

  uint64_t chunks[NUM_CHUNKS] = {};

public:
  void set(const int pos) {
    const int chunk = pos / BITS_PER_CHUNK;
    const int offset = pos % BITS_PER_CHUNK;
    chunks[chunk] |= (1ULL << offset);
  }

  void clear(const int pos) {
    const int chunk = pos / BITS_PER_CHUNK;
    const int offset = pos % BITS_PER_CHUNK;
    chunks[chunk] &= ~(1ULL << offset);
  }

  bool test(const int pos) const {
    const int chunk = pos / BITS_PER_CHUNK;
    const int offset = pos % BITS_PER_CHUNK;
    return chunks[chunk] & (1ULL << offset);
  }

 int find_next(int pos) const {
    const int chunk = pos / BITS_PER_CHUNK;
    const int offset = pos % BITS_PER_CHUNK;

    const uint64_t mask = ~((1ULL << offset) - 1);
    if (const uint64_t bits = chunks[chunk] & mask) {
      return chunk * BITS_PER_CHUNK + __builtin_ctzll(bits);
    }

    for (int i = chunk + 1; i < NUM_CHUNKS; ++i) {
      if (chunks[i]) {
        return i * BITS_PER_CHUNK + __builtin_ctzll(chunks[i]);
      }
    }

    return 4500;
  }

  int find_prev(int pos) const {
    const int chunk = pos / BITS_PER_CHUNK;
    const int offset = pos % BITS_PER_CHUNK;

    const uint64_t mask = (1ULL << (offset + 1)) - 1;
    if (const uint64_t bits = chunks[chunk] & mask) {
      return chunk * BITS_PER_CHUNK + (63 - __builtin_clzll(bits));
    }

    for (int i = chunk - 1; i >= 0; --i) {
      if (chunks[i]) {
        return i * BITS_PER_CHUNK + (63 - __builtin_clzll(chunks[i]));
      }
    }

    return 0;
  }
};
