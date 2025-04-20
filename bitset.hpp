#pragma once
#include <cstdint>

constexpr uint16_t PRICE_MAX = 4500;

class ChunkedBitset {
  static constexpr  uint16_t BITS_PER_CHUNK = 64;
  static constexpr  uint16_t MAX_BITS = PRICE_MAX;
  static constexpr  uint16_t NUM_CHUNKS = MAX_BITS / BITS_PER_CHUNK;

  uint64_t chunks[NUM_CHUNKS] = {};

public:
  void set(const  uint16_t pos) {
    const  uint16_t chunk = pos / BITS_PER_CHUNK;
    const  uint16_t offset = pos % BITS_PER_CHUNK;
    chunks[chunk] |= (1ULL << offset);
  }

  void clear(const  uint16_t pos) {
    const  uint16_t chunk = pos / BITS_PER_CHUNK;
    const  uint16_t offset = pos % BITS_PER_CHUNK;
    chunks[chunk] &= ~(1ULL << offset);
  }

  bool test(const  uint16_t pos) const {
    const  uint16_t chunk = pos / BITS_PER_CHUNK;
    const  uint16_t offset = pos % BITS_PER_CHUNK;
    return chunks[chunk] & (1ULL << offset);
  }

  uint16_t find_next( uint16_t pos) const {
    const  uint16_t chunk = pos / BITS_PER_CHUNK;
    const  uint16_t offset = pos % BITS_PER_CHUNK;

    const uint64_t mask = ~((1ULL << offset) - 1);
    if (const uint64_t bits = chunks[chunk] & mask) {
      return chunk * BITS_PER_CHUNK + __builtin_ctzll(bits);
    }

    for ( uint16_t i = chunk + 1; i < NUM_CHUNKS; ++i) {
      if (chunks[i]) {
        return i * BITS_PER_CHUNK + __builtin_ctzll(chunks[i]);
      }
    }

    return MAX_BITS;
  }

   uint16_t find_prev( uint16_t pos) const {
    const  uint16_t chunk = pos / BITS_PER_CHUNK;
    const  uint16_t offset = pos % BITS_PER_CHUNK;

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
