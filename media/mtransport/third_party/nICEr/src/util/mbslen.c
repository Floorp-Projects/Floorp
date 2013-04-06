/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifdef LINUX
#define _GNU_SOURCE 1
#endif
#include <string.h>

#include <errno.h>
#include <csi_platform.h>

#include <assert.h>
#include <locale.h>
#include <stdlib.h>
#include <wchar.h>
#ifdef DARWIN
#include <xlocale.h>
#endif /* DARWIN */

#include "nr_api.h"
#include "mbslen.h"

/* get number of characters in a mult-byte character string */
int
mbslen(const char *s, size_t *ncharsp)
{
#ifdef DARWIN
    static locale_t loc = 0;
    static int initialized = 0;
#endif /* DARWIN */
#ifdef WIN32
    char *my_locale=0;
    unsigned int i;
#endif  /* WIN32 */
    int _status;
    size_t nbytes;
    int nchars;
    mbstate_t mbs;

#ifdef DARWIN
    if (! initialized) {
        initialized = 1;
        loc = newlocale(LC_CTYPE_MASK, "UTF-8", LC_GLOBAL_LOCALE);
    }

    if (loc == 0) {
        /* unable to create the UTF-8 locale */
        assert(loc != 0);  /* should never happen */
#endif /* DARWIN */

#ifdef WIN32
    if (!setlocale(LC_CTYPE, 0))
        ABORT(R_INTERNAL);

    if (!(my_locale = r_strdup(setlocale(LC_CTYPE, 0))))
        ABORT(R_NO_MEMORY);

    for (i=0; i<strlen(my_locale); i++)
        my_locale[i] = toupper(my_locale[i]);

    if (!strstr(my_locale, "UTF-8") && !strstr(my_locale, "UTF8"))
        ABORT(R_NOT_FOUND);
#else
    /* can't count UTF-8 characters with mbrlen if the locale isn't UTF-8 */
    /* null-checking setlocale is required because Android */
    char *locale = setlocale(LC_CTYPE, 0);
    /* some systems use "utf8" instead of "UTF-8" like Fedora 17 */
    if (!locale || (!strcasestr(locale, "UTF-8") && !strcasestr(locale, "UTF8")))
        ABORT(R_NOT_FOUND);
#endif

#ifdef DARWIN
    }
#endif /* DARWIN */

    memset(&mbs, 0, sizeof(mbs));
    nchars = 0;

#ifdef DARWIN
    while (*s != '\0' && (nbytes = mbrlen_l(s, strlen(s), &mbs, loc)) != 0)
#else
    while (*s != '\0' && (nbytes = mbrlen(s, strlen(s), &mbs)) != 0)
#endif /* DARWIN */
    {
        if (nbytes == (size_t)-1)   /* should never happen */ {
            ABORT(R_INTERNAL);
        }
        if (nbytes == (size_t)-2)   /* encoding error */ {
            ABORT(R_BAD_DATA);
        }

        s += nbytes;
        ++nchars;
    }

    *ncharsp = nchars;

    _status = 0;
  abort:
#ifdef WIN32
    RFREE(my_locale);
#endif
    return _status;
}

