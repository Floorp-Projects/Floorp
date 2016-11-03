/*
 * Functions to trace SSL protocol behavior in DEBUG builds.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdarg.h>
#include "cert.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "prprf.h"

#if defined(DEBUG) || defined(TRACE)
static const char *hex = "0123456789abcdef";

static const char printable[257] = {
    "................"  /* 0x */
    "................"  /* 1x */
    " !\"#$%&'()*+,-./" /* 2x */
    "0123456789:;<=>?"  /* 3x */
    "@ABCDEFGHIJKLMNO"  /* 4x */
    "PQRSTUVWXYZ[\\]^_" /* 5x */
    "`abcdefghijklmno"  /* 6x */
    "pqrstuvwxyz{|}~."  /* 7x */
    "................"  /* 8x */
    "................"  /* 9x */
    "................"  /* ax */
    "................"  /* bx */
    "................"  /* cx */
    "................"  /* dx */
    "................"  /* ex */
    "................"  /* fx */
};

void
ssl_PrintBuf(const sslSocket *ss, const char *msg, const void *vp, int len)
{
    const unsigned char *cp = (const unsigned char *)vp;
    char buf[80];
    char *bp;
    char *ap;

    if (ss) {
        SSL_TRACE(("%d: SSL[%d]: %s [Len: %d]", SSL_GETPID(), ss->fd,
                   msg, len));
    } else {
        SSL_TRACE(("%d: SSL: %s [Len: %d]", SSL_GETPID(), msg, len));
    }

    if (!cp) {
        SSL_TRACE(("   <NULL>"));
        return;
    }

    memset(buf, ' ', sizeof buf);
    bp = buf;
    ap = buf + 50;
    while (--len >= 0) {
        unsigned char ch = *cp++;
        *bp++ = hex[(ch >> 4) & 0xf];
        *bp++ = hex[ch & 0xf];
        *bp++ = ' ';
        *ap++ = printable[ch];
        if (ap - buf >= 66) {
            *ap = 0;
            SSL_TRACE(("   %s", buf));
            memset(buf, ' ', sizeof buf);
            bp = buf;
            ap = buf + 50;
        }
    }
    if (bp > buf) {
        *ap = 0;
        SSL_TRACE(("   %s", buf));
    }
}

void
ssl_Trace(const char *format, ...)
{
    char buf[2000];
    va_list args;

    if (ssl_trace_iob) {
        va_start(args, format);
        PR_vsnprintf(buf, sizeof(buf), format, args);
        va_end(args);

        fputs(buf, ssl_trace_iob);
        fputs("\n", ssl_trace_iob);
    }
}

void
ssl_PrintKey(const sslSocket *ss, const char *msg, PK11SymKey *key)
{
    SECStatus rv;
    SECItem *rawkey;

    rv = PK11_ExtractKeyValue(key);
    if (rv != SECSuccess) {
        ssl_Trace("Could not extract key for %s", msg);
        return;
    }
    rawkey = PK11_GetKeyData(key);
    if (!rawkey) {
        ssl_Trace("Could not extract key for %s", msg);
        return;
    }
    ssl_PrintBuf(ss, msg, rawkey->data, rawkey->len);
}
#endif
