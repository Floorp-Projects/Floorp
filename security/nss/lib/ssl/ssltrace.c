/*
 * Functions to trace SSL protocol behavior in DEBUG builds.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <stdarg.h>
#include "cert.h"
#include "ssl.h"
#include "sslimpl.h"
#include "sslproto.h"
#include "prprf.h"

#if defined(DEBUG) || defined(TRACE)
static const char *hex = "0123456789abcdef";

static const char printable[257] = {
	"................"	/* 0x */
	"................"	/* 1x */
	" !\"#$%&'()*+,-./"	/* 2x */
	"0123456789:;<=>?"	/* 3x */
	"@ABCDEFGHIJKLMNO"	/* 4x */
	"PQRSTUVWXYZ[\\]^_"	/* 5x */
	"`abcdefghijklmno"	/* 6x */
	"pqrstuvwxyz{|}~."	/* 7x */
	"................"	/* 8x */
	"................"	/* 9x */
	"................"	/* ax */
	"................"	/* bx */
	"................"	/* cx */
	"................"	/* dx */
	"................"	/* ex */
	"................"	/* fx */
};

void ssl_PrintBuf(sslSocket *ss, const char *msg, const void *vp, int len)
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

#define LEN(cp)		(((cp)[0] << 8) | ((cp)[1]))

static void PrintType(sslSocket *ss, char *msg)
{
    if (ss) {
	SSL_TRACE(("%d: SSL[%d]: dump-msg: %s", SSL_GETPID(), ss->fd,
		   msg));
    } else {
	SSL_TRACE(("%d: SSL: dump-msg: %s", SSL_GETPID(), msg));
    }
}

static void PrintInt(sslSocket *ss, char *msg, unsigned v)
{
    if (ss) {
	SSL_TRACE(("%d: SSL[%d]:           %s=%u", SSL_GETPID(), ss->fd,
		   msg, v));
    } else {
	SSL_TRACE(("%d: SSL:           %s=%u", SSL_GETPID(), msg, v));
    }
}

/* PrintBuf is just like ssl_PrintBuf above, except that:
 * a) It prefixes each line of the buffer with "XX: SSL[xxx]           "
 * b) It dumps only hex, not ASCII.
 */
static void PrintBuf(sslSocket *ss, char *msg, unsigned char *cp, int len)
{
    char buf[80];
    char *bp;

    if (ss) {
	SSL_TRACE(("%d: SSL[%d]:           %s [Len: %d]", 
		   SSL_GETPID(), ss->fd, msg, len));
    } else {
	SSL_TRACE(("%d: SSL:           %s [Len: %d]", 
		   SSL_GETPID(), msg, len));
    }
    bp = buf;
    while (--len >= 0) {
	unsigned char ch = *cp++;
	*bp++ = hex[(ch >> 4) & 0xf];
	*bp++ = hex[ch & 0xf];
	*bp++ = ' ';
	if (bp + 4 > buf + 50) {
	    *bp = 0;
	    if (ss) {
		SSL_TRACE(("%d: SSL[%d]:             %s",
			   SSL_GETPID(), ss->fd, buf));
	    } else {
		SSL_TRACE(("%d: SSL:             %s", SSL_GETPID(), buf));
	    }
	    bp = buf;
	}
    }
    if (bp > buf) {
	*bp = 0;
	if (ss) {
	    SSL_TRACE(("%d: SSL[%d]:             %s",
		       SSL_GETPID(), ss->fd, buf));
	} else {
	    SSL_TRACE(("%d: SSL:             %s", SSL_GETPID(), buf));
	}
    }
}

void ssl_DumpMsg(sslSocket *ss, unsigned char *bp, unsigned len)
{
    switch (bp[0]) {
      case SSL_MT_ERROR:
	PrintType(ss, "Error");
	PrintInt(ss, "error", LEN(bp+1));
	break;

      case SSL_MT_CLIENT_HELLO:
	{
	    unsigned lcs = LEN(bp+3);
	    unsigned ls  = LEN(bp+5);
	    unsigned lc  = LEN(bp+7);

	    PrintType(ss, "Client-Hello");

	    PrintInt(ss, "version (Major)",                   bp[1]);
	    PrintInt(ss, "version (minor)",                   bp[2]);

	    PrintBuf(ss, "cipher-specs",         bp+9,        lcs);
	    PrintBuf(ss, "session-id",           bp+9+lcs,    ls);
	    PrintBuf(ss, "challenge",            bp+9+lcs+ls, lc);
	}
	break;
      case SSL_MT_CLIENT_MASTER_KEY:
	{
	    unsigned lck = LEN(bp+4);
	    unsigned lek = LEN(bp+6);
	    unsigned lka = LEN(bp+8);

	    PrintType(ss, "Client-Master-Key");

	    PrintInt(ss, "cipher-choice",                       bp[1]);
	    PrintInt(ss, "key-length",                          LEN(bp+2));

	    PrintBuf(ss, "clear-key",            bp+10,         lck);
	    PrintBuf(ss, "encrypted-key",        bp+10+lck,     lek);
	    PrintBuf(ss, "key-arg",              bp+10+lck+lek, lka);
	}
	break;
      case SSL_MT_CLIENT_FINISHED:
	PrintType(ss, "Client-Finished");
	PrintBuf(ss, "connection-id",            bp+1,          len-1);
	break;
      case SSL_MT_SERVER_HELLO:
	{
	    unsigned lc = LEN(bp+5);
	    unsigned lcs = LEN(bp+7);
	    unsigned lci = LEN(bp+9);

	    PrintType(ss, "Server-Hello");

	    PrintInt(ss, "session-id-hit",                     bp[1]);
	    PrintInt(ss, "certificate-type",                   bp[2]);
	    PrintInt(ss, "version (Major)",                    bp[3]);
	    PrintInt(ss, "version (minor)",                    bp[3]);
	    PrintBuf(ss, "certificate",          bp+11,        lc);
	    PrintBuf(ss, "cipher-specs",         bp+11+lc,     lcs);
	    PrintBuf(ss, "connection-id",        bp+11+lc+lcs, lci);
	}
	break;
      case SSL_MT_SERVER_VERIFY:
	PrintType(ss, "Server-Verify");
	PrintBuf(ss, "challenge",                bp+1,         len-1);
	break;
      case SSL_MT_SERVER_FINISHED:
	PrintType(ss, "Server-Finished");
	PrintBuf(ss, "session-id",               bp+1,         len-1);
	break;
      case SSL_MT_REQUEST_CERTIFICATE:
	PrintType(ss, "Request-Certificate");
	PrintInt(ss, "authentication-type",                    bp[1]);
	PrintBuf(ss, "certificate-challenge",    bp+2,         len-2);
	break;
      case SSL_MT_CLIENT_CERTIFICATE:
	{
	    unsigned lc = LEN(bp+2);
	    unsigned lr = LEN(bp+4);
	    PrintType(ss, "Client-Certificate");
	    PrintInt(ss, "certificate-type",                   bp[1]);
	    PrintBuf(ss, "certificate",          bp+6,         lc);
	    PrintBuf(ss, "response",             bp+6+lc,      lr);
	}
	break;
      default:
	ssl_PrintBuf(ss, "sending *unknown* message type", bp, len);
	return;
    }
}

void
ssl_Trace(const char *format, ... )
{
    char buf[2000]; 
    va_list args;

    if (ssl_trace_iob) {
	va_start(args, format);
	PR_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	fputs(buf,  ssl_trace_iob);
	fputs("\n", ssl_trace_iob);
    }
}
#endif
