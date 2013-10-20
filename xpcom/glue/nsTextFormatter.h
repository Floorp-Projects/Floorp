/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This code was copied from xpcom/ds/nsTextFormatter r1.3
 *           Memory model and Frozen linkage changes only.
 *                           -- Prasad <prasad@medhas.org>
 */

#ifndef nsTextFormatter_h___
#define nsTextFormatter_h___

/*
 ** API for PR printf like routines. Supports the following formats
 **	%d - decimal
 **	%u - unsigned decimal
 **	%x - unsigned hex
 **	%X - unsigned uppercase hex
 **	%o - unsigned octal
 **	%hd, %hu, %hx, %hX, %ho - 16-bit versions of above
 **	%ld, %lu, %lx, %lX, %lo - 32-bit versions of above
 **	%lld, %llu, %llx, %llX, %llo - 64 bit versions of above
 **	%s - utf8 string
 **	%S - PRUnichar string
 **	%c - character
 **	%p - pointer (deals with machine dependent pointer size)
 **	%f - float
 **	%g - float
 */
#include "prio.h"
#include <stdio.h>
#include <stdarg.h>
#include "nscore.h"
#include "nsStringGlue.h"

#ifdef XPCOM_GLUE
#error "nsTextFormatter is not available in the standalone glue due to NSPR dependencies."
#endif

class NS_COM_GLUE nsTextFormatter {

  public:

    /*
     * sprintf into a fixed size buffer. Guarantees that the buffer is null
     * terminated. Returns the length of the written output, NOT including the
     * null terminator, or (uint32_t)-1 if an error occurs.
     */
    static uint32_t snprintf(PRUnichar *out, uint32_t outlen, const PRUnichar *fmt, ...);

    /*
     * sprintf into a nsMemory::Alloc'd buffer. Return a pointer to 
     * buffer on success, nullptr on failure. 
     */
    static PRUnichar* smprintf(const PRUnichar *fmt, ...);

    static uint32_t ssprintf(nsAString& out, const PRUnichar* fmt, ...);

    /*
     * va_list forms of the above.
     */
    static uint32_t vsnprintf(PRUnichar *out, uint32_t outlen, const PRUnichar *fmt, va_list ap);
    static PRUnichar* vsmprintf(const PRUnichar *fmt, va_list ap);
    static uint32_t vssprintf(nsAString& out, const PRUnichar *fmt, va_list ap);

    /*
     * Free the memory allocated, for the caller, by smprintf.
     * -- Deprecated --
     * Callers can substitute calling smprintf_free with nsMemory::Free
     */
    static void smprintf_free(PRUnichar *mem);

};

#endif /* nsTextFormatter_h___ */
