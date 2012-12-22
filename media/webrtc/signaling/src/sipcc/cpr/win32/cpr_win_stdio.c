/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "cpr_types.h"
#include "cpr_win_stdio.h"
#include "CSFLog.h"

typedef enum
{
    CPR_LOGLEVEL_ERROR=0,
    CPR_LOGLEVEL_WARNING,
    CPR_LOGLEVEL_INFO,
    CPR_LOGLEVEL_DEBUG
} cpr_log_level_e;


/**
 * @def LOG_MAX
 *
 * Constant represents the maximum allowed length for a message
 */
#define LOG_MAX 4096


int32_t
buginf_msg (const char *str)
{
    OutputDebugStringA(str);

    return (0);
}

void
err_msg (const char *_format, ...)
{
  va_list ap;

  va_start(ap, _format);
  CSFLogErrorV("cpr", _format, ap);
  va_end(ap);
}

void
notice_msg (const char *_format, ...)
{
  va_list ap;

  va_start(ap, _format);
  CSFLogInfoV("cpr", _format, ap);
  va_end(ap);
}

int32_t
buginf (const char *_format, ...)
{
  va_list ap;

  va_start(ap, _format);
  CSFLogDebugV("cpr", _format, ap);
  va_end(ap);

  return (0);
}

int
cpr_win_snprintf(char *buffer, size_t n, const char *format, ...)
{
  va_list argp;
  int ret;
  va_start(argp, format);
  ret = _vscprintf(format, argp);
  vsnprintf_s(buffer, n, _TRUNCATE, format, argp);
  va_end(argp);
  return ret;
}
