/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_LOG
#define CUBEB_LOG

#ifdef __cplusplus
extern "C" {
#endif

extern cubeb_log_level g_log_level;
extern cubeb_log_callback g_log_callback;

#ifdef __cplusplus
}
#endif

#define LOGV(msg, ...) LOG_INTERNAL(CUBEB_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define LOG(msg, ...) LOG_INTERNAL(CUBEB_LOG_NORMAL, msg, ##__VA_ARGS__)

#define LOG_INTERNAL(level, fmt, ...) do {                                   \
    if (g_log_callback && level <= g_log_level) {                            \
      g_log_callback("%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    }                                                                        \
  } while(0)

#endif // CUBEB_LOG
