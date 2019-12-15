#pragma once

class worker {
public:
  worker() = default;
  virtual ~worker() = default;
  worker(worker&) = delete;
  worker(worker&&) = delete;
  worker& operator=(worker&) = delete;
  worker& operator=(worker&&) = delete;

  virtual void run() = 0;
};