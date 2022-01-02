/* Simple Plugin API
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

#ifndef SPA_LOG_H
#define SPA_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#include <spa/utils/defs.h>
#include <spa/utils/hook.h>

enum spa_log_level {
	SPA_LOG_LEVEL_NONE = 0,
	SPA_LOG_LEVEL_ERROR,
	SPA_LOG_LEVEL_WARN,
	SPA_LOG_LEVEL_INFO,
	SPA_LOG_LEVEL_DEBUG,
	SPA_LOG_LEVEL_TRACE,
};

/**
 * The Log interface
 */
#define SPA_TYPE_INTERFACE_Log	SPA_TYPE_INFO_INTERFACE_BASE "Log"

#define SPA_VERSION_LOG		0

struct spa_log {
	/** the version of this log. This can be used to expand this
	 * structure in the future */
	struct spa_interface iface;
	/**
	 * Logging level, everything above this level is not logged
	 */
	enum spa_log_level level;
};

struct spa_log_methods {
#define SPA_VERSION_LOG_METHODS	0
	uint32_t version;
	/**
	 * Log a message with the given log level.
	 *
	 * \param log a spa_log
	 * \param level a spa_log_level
	 * \param file the file name
	 * \param line the line number
	 * \param func the function name
	 * \param fmt printf style format
	 * \param ... format arguments
	 */
	void (*log) (void *object,
		     enum spa_log_level level,
		     const char *file,
		     int line,
		     const char *func,
		     const char *fmt, ...) SPA_PRINTF_FUNC(6, 7);

	/**
	 * Log a message with the given log level.
	 *
	 * \param log a spa_log
	 * \param level a spa_log_level
	 * \param file the file name
	 * \param line the line number
	 * \param func the function name
	 * \param fmt printf style format
	 * \param args format arguments
	 */
	void (*logv) (void *object,
		      enum spa_log_level level,
		      const char *file,
		      int line,
		      const char *func,
		      const char *fmt,
		      va_list args) SPA_PRINTF_FUNC(6, 0);
};

#define spa_log_level_enabled(l,lev) ((l) && (l)->level >= (lev))

#if defined(__USE_ISOC11) || defined(__USE_ISOC99) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)

#define spa_log_log(l,lev,...)					\
({								\
	struct spa_log *_l = l;					\
	if (SPA_UNLIKELY(spa_log_level_enabled(_l, lev)))	\
		spa_interface_call(&_l->iface,			\
			struct spa_log_methods, log, 0, lev,	\
			__VA_ARGS__);				\
})

#define spa_log_logv(l,lev,...)					\
({								\
	struct spa_log *_l = l;					\
	if (SPA_UNLIKELY(spa_log_level_enabled(_l, lev)))	\
		spa_interface_call(&_l->iface,			\
			struct spa_log_methods, logv, 0, lev,	\
			__VA_ARGS__);				\
})

#define spa_log_error(l,...)	spa_log_log(l,SPA_LOG_LEVEL_ERROR,__FILE__,__LINE__,__func__,__VA_ARGS__)
#define spa_log_warn(l,...)	spa_log_log(l,SPA_LOG_LEVEL_WARN,__FILE__,__LINE__,__func__,__VA_ARGS__)
#define spa_log_info(l,...)	spa_log_log(l,SPA_LOG_LEVEL_INFO,__FILE__,__LINE__,__func__,__VA_ARGS__)
#define spa_log_debug(l,...)	spa_log_log(l,SPA_LOG_LEVEL_DEBUG,__FILE__,__LINE__,__func__,__VA_ARGS__)
#define spa_log_trace(l,...)	spa_log_log(l,SPA_LOG_LEVEL_TRACE,__FILE__,__LINE__,__func__,__VA_ARGS__)

#ifndef FASTPATH
#define spa_log_trace_fp(l,...)	spa_log_log(l,SPA_LOG_LEVEL_TRACE,__FILE__,__LINE__,__func__,__VA_ARGS__)
#else
#define spa_log_trace_fp(l,...)
#endif

#else

#define SPA_LOG_FUNC(name,lev)							\
static inline SPA_PRINTF_FUNC(2,3) void spa_log_##name (struct spa_log *l, const char *format, ...)  \
{										\
	if (SPA_UNLIKELY(spa_log_level_enabled(l, lev))) {			\
		va_list varargs;						\
		va_start (varargs, format);					\
		spa_interface_call(&l->iface,					\
			struct spa_log_methods, logv, 0, lev,			\
			__FILE__,__LINE__,__func__,format,varargs);		\
		va_end (varargs);						\
	}									\
}

SPA_LOG_FUNC(error, SPA_LOG_LEVEL_ERROR)
SPA_LOG_FUNC(warn, SPA_LOG_LEVEL_WARN)
SPA_LOG_FUNC(info, SPA_LOG_LEVEL_INFO)
SPA_LOG_FUNC(debug, SPA_LOG_LEVEL_DEBUG)
SPA_LOG_FUNC(trace, SPA_LOG_LEVEL_TRACE)

#ifndef FASTPATH
SPA_LOG_FUNC(trace_fp, SPA_LOG_LEVEL_TRACE)
#else
static inline void spa_log_trace_fp (struct spa_log *l, const char *format, ...) { }
#endif

#endif

/** keys can be given when initializing the logger handle */
#define SPA_KEY_LOG_LEVEL		"log.level"		/**< the default log level */
#define SPA_KEY_LOG_COLORS		"log.colors"		/**< enable colors in the logger */
#define SPA_KEY_LOG_FILE		"log.file"		/**< log to the specified file instead of
								  *  stderr. */
#define SPA_KEY_LOG_TIMESTAMP		"log.timestamp"		/**< log timestamps */
#define SPA_KEY_LOG_LINE		"log.line"		/**< log file and line numbers */

#ifdef __cplusplus
}  /* extern "C" */
#endif
#endif /* SPA_LOG_H */
