#pragma once
#include <string>
#include <regex>

namespace util {

#ifdef WIN32
#include <windows.h>

static size_t sys_free_mem_bytes()
{
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  if (!::GlobalMemoryStatusEx(&status))
    return 0;
  return status.ullAvailPhys;
}

static size_t sys_free_disk_bytes(std::string path) {
  ULARGE_INTEGER lpFreeBytesAvailableToCaller;
  ULARGE_INTEGER lpTotalNumberOfBytes;
  ULARGE_INTEGER lpTotalNumberOfFreeBytes;
  if (!::GetDiskFreeSpaceExA(path.c_str()
                      , &lpFreeBytesAvailableToCaller
                      , &lpTotalNumberOfBytes
                      , &lpTotalNumberOfFreeBytes))
    return 0;

  return (size_t)lpFreeBytesAvailableToCaller.QuadPart;
}

static bool acquire_manage_volume_privs() {
  HANDLE token;
  if (!OpenProcessToken(GetCurrentProcess() , TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    return false;

  TOKEN_PRIVILEGES privs{};
  if (!LookupPrivilegeValue(nullptr, SE_MANAGE_VOLUME_NAME , &privs.Privileges[0].Luid))
    return false;

  privs.PrivilegeCount = 1;
  privs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  if(!AdjustTokenPrivileges(token, 0, &privs, 0, nullptr, nullptr))
		return false;

  auto ec = GetLastError();
  if(ec == ERROR_NOT_ALL_ASSIGNED)
		return false;
  return true;
}

static uint32_t get_volume_sn(const std::string& driver) {
  DWORD lpVolumeSerialNumber{0};
  auto res = GetVolumeInformationA(driver.empty() ? nullptr : driver.c_str()
                                 , nullptr, 0, &lpVolumeSerialNumber
                                 , nullptr, nullptr, nullptr, 0);
  if (res == TRUE)
    return lpVolumeSerialNumber;
  return -1;
}
#else
#endif

static std::vector<std::string> split(const std::string& src, const std::string& delim) {
  if (src.empty())
    return {};

  std::regex re{delim};
  auto res = std::vector<std::string> {
    std::sregex_token_iterator(src.begin(), src.end(), re, -1),
    std::sregex_token_iterator()
  };

  return res;
}

}