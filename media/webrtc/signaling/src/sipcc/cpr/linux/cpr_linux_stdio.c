/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_stdio.h"
#include "cpr_string.h"
#include "CSFLog.h"

/**
 * @def LOG_MAX
 *
 * Constant represents the maximum allowed length for a message
 */
#define LOG_MAX 1024

/**
 * @addtogroup DebugAPIs The CPR Logging Abstractions
 * @ingroup CPR
 * @brief The CPR Debug/Logging APIs
 *
 * @{
 */

/**
 * Debug message
 *
 * @param _format  format string
 * @param ...      variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
int
buginf (const char *_format, ...)
{
    char fmt_buf[LOG_MAX + 1];
    va_list ap;
    int rc;

    va_start(ap, _format);
    rc = vsnprintf(fmt_buf, LOG_MAX, _format, ap);
    va_end(ap);
    if (rc <= 0) {
        return rc;
    }

  CSFLogDebug("cpr", "%s", fmt_buf);

  return rc;
}

/**
 * Debug message that can be larger than #LOG_MAX
 *
 * @param str - a fixed constant string
 *
 * @return  zero(0)
 *
 * @pre (str not_eq NULL)
 */
int
buginf_msg (const char *str)
{
    char buf[LOG_MAX + 1];
    const char *p;
    int16_t len;

    // terminate buffer
    buf[LOG_MAX] = NUL;

    len = (int16_t) strlen(str);

    if (len > LOG_MAX) {
        p = str;
        do {
            memcpy(buf, p, LOG_MAX);
            p += LOG_MAX;
            len -= LOG_MAX;

            printf("%s",buf);
        } while (len > LOG_MAX);

        if (len) {
          CSFLogDebug("cpr", "%s", (char *)p);
        }
    } else {
      CSFLogDebug("cpr", "%s", (char *) str);
    }

    return 0;
}

/**
 * Error message
 *
 * @param _format  format string
 * @param ...     variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
void
err_msg (const char *_format, ...)
{
    char fmt_buf[LOG_MAX + 1];
    va_list ap;
    int rc;

    va_start(ap, _format);
    rc = vsnprintf(fmt_buf, LOG_MAX, _format, ap);
    va_end(ap);
    if (rc <= 0) {
        return;
    }

    CSFLogError("cpr", "%s", fmt_buf);
}


/**
 * Notice message
 *
 * @param _format  format string
 * @param ...     variable arg list
 *
 * @return  Return code from vsnprintf
 *
 * @pre (_format not_eq NULL)
 */
void
notice_msg (const char *_format, ...)
{
    char fmt_buf[LOG_MAX + 1];
    va_list ap;
    int rc;

    va_start(ap, _format);
    rc = vsnprintf(fmt_buf, LOG_MAX, _format, ap);
    va_end(ap);
    if (rc <= 0) {
        return;
    }

    CSFLogInfo("cpr", "%s", fmt_buf);
}

