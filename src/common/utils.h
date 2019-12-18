#pragma once

namespace util {

#ifdef WIN32
#include <windows.h>

static size_t sys_free_mem_bytes()
{
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  return status.ullAvailPhys;
}
#else
#endif

}