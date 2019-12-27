#pragma once

#include <cstdint>
#include <string>
#include <array>

using plot_id_t = std::array<uint8_t, 20>;

struct plotter_base {
  static constexpr int HASH_SIZE = 32;
  static constexpr int HASHES_PER_SCOOP = 2;
  static constexpr int SCOOP_SIZE = HASHES_PER_SCOOP * HASH_SIZE;
  static constexpr int SCOOPS_PER_PLOT = 4096;
  static constexpr int PLOT_SIZE = SCOOPS_PER_PLOT * SCOOP_SIZE;
  static constexpr int SEED_LENGTH = 28;
  static constexpr int PLOT_TOTAL_SIZE = PLOT_SIZE + SEED_LENGTH;
  static constexpr int HASH_CAP = 4096;
  static constexpr char SEED_MAGIC[] = "\0\0\0\0";

  static constexpr char HEX_CHARS[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                        'a', 'b', 'c', 'd', 'e', 'f' };
  static constexpr char HEX_CHARS2[] = {
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, '-', '-', '-', '-', '-', '-', '-',
    10, 11, 12, 13, 14, 15, '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    10, 11, 12, 13, 14, 15, '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-'
  };
  static_assert(sizeof(HEX_CHARS2) == 128, "invalid HEX_CHARS2");
  static_assert(HEX_CHARS2['0'] == 0x00, "invalid HEX_CHARS2");
  static_assert(HEX_CHARS2['a'] == 0x0a, "invalid HEX_CHARS2");
  static_assert(HEX_CHARS2['A'] == 0x0a, "invalid HEX_CHARS2");

  static inline uint64_t hton_ull(uint64_t n)
  {
    return (((n & 0xff00000000000000ull) >> 56)
          | ((n & 0x00ff000000000000ull) >> 40)
          | ((n & 0x0000ff0000000000ull) >> 24)
          | ((n & 0x000000ff00000000ull) >> 8)
          | ((n & 0x00000000ff000000ull) << 8)
          | ((n & 0x0000000000ff0000ull) << 24)
          | ((n & 0x000000000000ff00ull) << 40)
          | ((n & 0x00000000000000ffull) << 56));
  }

  static std::string btoh(const uint8_t* data, size_t size) {
    std::string hex;
    hex.resize(size * 2);
    uint8_t* p = (uint8_t*)hex.data();
    for (size_t i = 0; i < size; ++i) {
      *p++ = HEX_CHARS[(data[i] >> 4) & 0x0f];
      *p++ = HEX_CHARS[(data[i]) & 0x0f];
    }

    return hex;
  }

  static std::pair<bool, plot_id_t> to_plot_id_bytes(std::string plot_id_hex) {
    plot_id_t bytes{0};
    assert(plot_id_hex.size() == 40);
    if (plot_id_hex.size() != 40)
      return {false, bytes};
    auto data = plot_id_hex.data();
    for (auto i=0; i<plot_id_hex.size(); ++i,++i) {
      auto h = HEX_CHARS2[data[i]];
      auto l = HEX_CHARS2[data[i + 1]];
      if (h == '-' || l == '-')
        return {false, bytes};
      bytes[i >> 1] = (h << 4) | l;
    }
    return {true, bytes};
  }

};