/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __OGGZ_MACROS_H__
#define __OGGZ_MACROS_H__

#include <stdlib.h>
#include <ogg/ogg.h>

/* Use the malloc and free used by ogg; defaults are those from stdlib */
#define oggz_malloc _ogg_malloc
#define oggz_realloc _ogg_realloc
#define oggz_free _ogg_free

#undef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))

#undef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))

/*
** Inspiration : http://sourcefrog.net/weblog/software/languages/C/unused.html
*/
#ifdef UNUSED
#elif defined (__GNUC__)
#       define UNUSED(x) UNUSED_ ## x __attribute__ ((unused))
#elif defined (__LCLINT__)
#       define UNUSED(x) /*@unused@*/ x
#else
#       define UNUSED(x) x
#endif

#ifdef __GNUC__
#       define WARN_UNUSED      __attribute__ ((warn_unused_result))
#else
#       define WARN_UNUSED
#endif

#endif /* __OGGZ_MACROS_H__ */
