/*
   Copyright (C) 2007 Commonwealth Scientific and Industrial Research
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

#ifndef __OGGZ_OFF_T_GENERATED_H__
#define __OGGZ_OFF_T_GENERATED_H__

/** \file
 * Architecture-dependent type for oggz_off_t.
 *
 * This file should never be included directly by user code. Please include
 * either of <oggz/oggz.h> or <oggz/oggz_off_t.h> instead.
 *
 * This file was generated when liboggz was built.
 *
 * Note that this file is only generated when using GNU autoconf.
 * This file is not used on Win32 systems.
 */

/**
 * This typedef was determined on the system on which the documentation
 * was generated.
 *
 * To query this on your system, do eg.
 *
 <pre>
   echo "gcc -E oggz.h | grep oggz_off_t
 </pre>
 * 
 */

#include <sys/types.h>

#if defined(__APPLE__) || defined(SOLARIS)
typedef off_t oggz_off_t;
#else
typedef loff_t oggz_off_t;
#endif

#define PRI_OGGZ_OFF_T "PRId64"

#endif /* __OGGZ_OFF_T_GENERATED__ */
