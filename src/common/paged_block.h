#pragma once

#include <list>
#include <stdint.h>
#include <cstdlib>

namespace util {

class paged_block {
  static constexpr std::align_val_t page_size{4096};
  static constexpr size_t page_mask{((uint64_t)-1) ^ (4095)};

public:
  explicit paged_block() : data_(nullptr), len_(0) {}
  explicit paged_block(size_t len) {
    data_ =(uint8_t*) operator new[](len, page_size);
    len_ = len;
  }
  
  virtual ~paged_block() {
    if (data_)
      operator delete[](data_, page_size);
  }

  paged_block(paged_block& rh) = delete;
  paged_block& operator=(paged_block& rh) = delete;

  paged_block(paged_block&& rh) {
    data_ = rh.data_;
    len_ = rh.len_;
    rh.data_ = nullptr;
    rh.len_ = 0;
  }


  paged_block& operator=(paged_block&& rh) {
    data_ = rh.data_;
    len_ = rh.len_;
    rh.data_ = nullptr;
    rh.len_ = 0;
    return *this;
  }

  uint8_t* data() const { return data_; }
  size_t len() const { return len_; }
  operator bool() const { return data_ && len_; }

private:
  uint8_t* data_;
  size_t len_;
};

}