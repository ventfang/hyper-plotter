#pragma once

#include <atomic>
#include <spdlog/spdlog.h>

#ifdef WIN32
#include <Windows.h>
static BOOL WINAPI signal_handle(DWORD dwCtrlType);
#else
#include <signal.h>
static void signal_handle(int sig);
#endif

class signal {
public:
  static signal& get() {
    static signal signal_;
	return signal_;
  }

  void install_signal() {
    #ifdef WIN32
    SetConsoleCtrlHandler(signal_handle, true);
    #else
    signal(SIGINT, signal_handle);
    signal(SIGTERM, signal_handle);
    #endif
  }

  void signal_stop() { sig_stop_.store(true); }

  bool stopped() { sig_stop_.load(); }

private:
  std::atomic<bool> sig_stop_;
};

#ifdef WIN32
#include <Windows.h>
static BOOL WINAPI signal_handle(DWORD dwCtrlType)
{
	signal::get().signal_stop();
	spdlog::info("signal handled {}, stopping...", dwCtrlType);
	return TRUE;
}
#else
#include <signal.h>
static void signal_handle(int sig)
{
	signal::get().sig_stop();
	spdlog::info("signal handled {} ({}) stopping...", sig, strsignal(sig));
}
#endif