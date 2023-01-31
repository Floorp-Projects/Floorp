#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * Based on code from sys-info: https://crates.io/crates/sys-info
 * Original licenses: MIT
 * Original author: Siyu Wang
 * License file: https://github.com/FillZpp/sys-info-rs/blob/master/LICENSE
 */

/**
 * Get the OS release version.
 *
 * This uses [`GetVersionEx`] internally.
 * Depending on how the application is compiled
 * it might return different values for the same operating system.
 *
 * On Windows 10 the example will return the Windows 8 OS version value `6.2`.
 * If compiled into an application which explicitly targets Windows 10, it will
 * return `10.0`.
 *
 * [`GetVersionEx`]: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversionexa
 *
 * ## Arguments
 *
 * * `outbuf` - Pointer to an allocated buffer of size `outlen`.
 *              The OS version will be written here, null-terminated.
 *              If the version is longer than the buffer it will be truncated.
 * * `outlen` - Size of the buffer.
 *
 * ## Return value
 *
 * Returns the number of bytes written.
 */
int get_os_release(char *outbuf, size_t outlen) {
  assert(outlen > 1);

  OSVERSIONINFO osvi;

  ZeroMemory(&osvi, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);

  int written = 0;
  if (GetVersionEx(&osvi)) {
    written = snprintf(outbuf, outlen, "%ld.%ld", osvi.dwMajorVersion,
                       osvi.dwMinorVersion);
  } else {
    int res = strncpy_s(outbuf, outlen, "unknown", strlen("unknown"));
    if (res != 0) {
      return 0;
    }
    written = strlen("unknown");
  }

  // If the output buffer is smaller than the version (or "unknown"),
  // we only wrote until the buffer was full.
  return min(written, (int)outlen - 1);
}

/**
 * Get the windows build version.
 *
 * Works similarly to `get_os_release`, but returns the `dwBuildNumber` field from the 
 * struct returned by `GetVersionEx`.
 *
 * [`GetVersionEx`]: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversionexa
 *
 * ## Return value
 *
 * Returns the build number as an int.
 */
int get_build_number() {
  OSVERSIONINFO osvi;

  ZeroMemory(&osvi, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);

  if (GetVersionEx(&osvi)) {
    return osvi.dwBuildNumber;
  }    

  return 0;
}

