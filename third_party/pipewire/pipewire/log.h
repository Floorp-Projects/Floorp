/* PipeWire
 *
 * Copyright © 2018 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef PIPEWIRE_LOG_H
#define PIPEWIRE_LOG_H

#include <spa/support/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \class pw_log
 *
 * Logging functions of PipeWire
 *
 * Logging is performed to stdout and stderr. Trace logging is performed
 * in a lockfree ringbuffer and written out from the main thread as to not
 * block the realtime threads.
 */

/** The global log level */
extern enum spa_log_level pw_log_level;

/** Configure a logging module. This is usually done automatically
 * in pw_init() but you can install a custom logger before calling
 * pw_init(). */
void pw_log_set(struct spa_log *log);

/** Get the log interface */
struct spa_log *pw_log_get(void);

/** Configure the logging level */
void pw_log_set_level(enum spa_log_level level);


/** Log a message */
void
pw_log_log(enum spa_log_level level,
	   const char *file,
	   int line, const char *func,
	   const char *fmt, ...) SPA_PRINTF_FUNC(5, 6);

/** Log a message */
void
pw_log_logv(enum spa_log_level level,
	    const char *file,
	    int line, const char *func,
	    const char *fmt, va_list args) SPA_PRINTF_FUNC(5, 0);


/** Check if a loglevel is enabled \memberof pw_log */
#define pw_log_level_enabled(lev) (pw_log_level >= (lev))

#define pw_log(lev,...)							\
({									\
	if (SPA_UNLIKELY(pw_log_level_enabled (lev)))			\
		pw_log_log(lev,__FILE__,__LINE__,__func__,__VA_ARGS__);	\
})

#define pw_log_error(...)   pw_log(SPA_LOG_LEVEL_ERROR,__VA_ARGS__)
#define pw_log_warn(...)    pw_log(SPA_LOG_LEVEL_WARN,__VA_ARGS__)
#define pw_log_info(...)    pw_log(SPA_LOG_LEVEL_INFO,__VA_ARGS__)
#define pw_log_debug(...)   pw_log(SPA_LOG_LEVEL_DEBUG,__VA_ARGS__)
#define pw_log_trace(...)   pw_log(SPA_LOG_LEVEL_TRACE,__VA_ARGS__)

#ifndef FASTPATH
#define pw_log_trace_fp(...)   pw_log(SPA_LOG_LEVEL_TRACE,__VA_ARGS__)
#else
#define pw_log_trace_fp(...)
#endif

#ifdef __cplusplus
}
#endif
#endif /* PIPEWIRE_LOG_H */
