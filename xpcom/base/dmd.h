/*
   ----------------------------------------------------------------
   The following BSD-style license applies to this one file (dmd.h) only.
   ----------------------------------------------------------------

   The Initial Developer of the Original Code is
   the Mozilla Foundation.
   Portions created by the Initial Developer are Copyright (C) 2011
   the Initial Developer. All Rights Reserved.

   Contributor(s):
     Nicholas Nethercote <nnethercote@mozilla.com>

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. The origin of this software must not be misrepresented; you must 
      not claim that you wrote the original software.  If you use this 
      software in a product, an acknowledgment in the product 
      documentation would be appreciated but is not required.

   3. Altered source versions must be plainly marked as such, and must
      not be misrepresented as being the original software.

   4. The name of the author may not be used to endorse or promote 
      products derived from this software without specific prior written 
      permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
   OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __DMD_H
#define __DMD_H

#include "valgrind/valgrind.h"

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !! 
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end. */
typedef
   enum { 
      VG_USERREQ__DMD_REPORT = VG_USERREQ_TOOL_BASE('D','M'),
      VG_USERREQ__DMD_UNREPORT,
      VG_USERREQ__DMD_CHECK_REPORTING
   } Vg_DMDClientRequest;


/* Mark heap block at _qzz_addr as reported for _qzz_len bytes.
 * _qzz_name is the name of the reporter. */
#define VALGRIND_DMD_REPORT(_qzz_addr,_qzz_len,_qzz_name)        \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__DMD_REPORT,              \
                            (_qzz_addr), (_qzz_len), (_qzz_name), 0, 0)

/* Mark heap block at _qzz_addr as not reported. */
#define VALGRIND_DMD_UNREPORT(_qzz_addr)                         \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__DMD_UNREPORT,            \
                            (_qzz_addr), 0, 0, 0, 0)

/* Do a reporting check. */
#define VALGRIND_DMD_CHECK_REPORTING                             \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,      \
                            VG_USERREQ__DMD_CHECK_REPORTING,     \
                            0, 0, 0, 0, 0)

#endif

