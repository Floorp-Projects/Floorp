#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * Based on code from sys-info: https://crates.io/crates/sys-info
 * Original licenses: MIT
 * Original author: Siyu Wang
 * License file: https://github.com/FillZpp/sys-info-rs/blob/master/LICENSE
 */

#define LEN 20

/**
 * Get the OS release version.
 *
 * This uses [`GetVersionEx`] internally.
 * Depending on how the application is compiled
 * it might return different values for the same operating system.
 *
 * On Windows 10 the example will return the Windows 8 OS version value `6.2`.
 * If compiled into an application which explicitly targets Windows 10, it will return `10.0`.
 *
 * [`GetVersionEx`]: https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversionexa
 *
 * ## Return value
 *
 * Returns an allocated string representing the version number or the value "unknown" if an error occured.
 * The caller is responsible for freeing the allocated string, use `str_free` to do so.
 */
const char *get_os_release(void) {
  OSVERSIONINFO osvi;
  char *s = malloc(LEN);

  ZeroMemory(&osvi, sizeof(osvi));
  osvi.dwOSVersionInfoSize = sizeof(osvi);

  if (GetVersionEx(&osvi)) {
    snprintf(s, LEN, "%ld.%ld",
        osvi.dwMajorVersion, osvi.dwMinorVersion);
  } else {
    strncpy(s, "unknown", LEN);
  }
  s[LEN - 1] = '\0';

  return s;
}

void str_free(char *ptr) {
  free(ptr);
}
