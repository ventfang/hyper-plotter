#pragma once

#include "plotter_base.h"
#include "shabal/shabal.h"

#define BYTES_SWAP_64(a, b) do {\
                              auto t = *((uint64_t*)(a));\
                              *((uint64_t*)(a)) = *((uint64_t*)(b));\
                              *((uint64_t*)(b)) = t;\
                            } while(0)

struct cpu_plotter : public plotter_base {
  cpu_plotter() = default;
  cpu_plotter(cpu_plotter&) = delete;
  cpu_plotter(cpu_plotter&&) = delete;
  cpu_plotter& operator=(cpu_plotter&) = delete;
  cpu_plotter& operator=(cpu_plotter&&) = delete;

  void plot(plot_id_t plot_id, uint64_t nonce) {
    *((uint64_t*)(data_ + PLOT_SIZE + 0))  = hton_ull(nonce);
    *((uint64_t*)(data_ + PLOT_SIZE + 8))  = *((uint64_t*)(plot_id.data() + 0));
    *((uint64_t*)(data_ + PLOT_SIZE + 16)) = *((uint64_t*)(plot_id.data() + 8));
    *((uint32_t*)(data_ + PLOT_SIZE + 24)) = *((uint32_t*)(plot_id.data() + 16));
    *((uint32_t*)(data_ + PLOT_SIZE + 28)) = *((uint32_t*)(SEED_MAGIC));
    
    int len;
    Shabal256 shabal256;
    uint8_t hfinal[HASH_SIZE];
    for (int i = PLOT_SIZE; i > 0; i -= HASH_SIZE) {
      len = PLOT_TOTAL_SIZE - i;
      if (len > HASH_CAP) {
        len = HASH_CAP;
      }
      shabal256.update(data_ + i, len);
      shabal256.digest(data_ + i - HASH_SIZE);
    }
    shabal256.update(data_, PLOT_TOTAL_SIZE);
    shabal256.close(hfinal);

    for (int i = 0; i < PLOT_SIZE; ++i) {
      data_[i] ^= hfinal[i & 0x1f];
    }

    for (int i = 32, j = PLOT_SIZE - HASH_SIZE; i < (PLOT_SIZE / 2); i += 64, j -= 64) {
      BYTES_SWAP_64(&data_[i + 0], &data_[j + 0]);
      BYTES_SWAP_64(&data_[i + 8], &data_[j + 8]);
      BYTES_SWAP_64(&data_[i + 16], &data_[j + 16]);
      BYTES_SWAP_64(&data_[i + 24], &data_[j + 24]);
    }
  }

  std::string to_string(size_t size = PLOT_SIZE) const { return btoh(data_, size); }
  const uint8_t* data() const { return data_; }
  size_t size() const { return PLOT_SIZE; }

private:
  uint8_t data_[PLOT_TOTAL_SIZE + 1];
};