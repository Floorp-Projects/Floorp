/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is nsTraceMalloc.c/bloatblame.c code, released
 * April 19, 2000.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Brendan Eich, 14-April-2000
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */
#ifndef nsTraceMalloc_h___
#define nsTraceMalloc_h___

#include "prtypes.h"

PR_BEGIN_EXTERN_C

#define NS_TRACE_MALLOC_LOGFILE_MAGIC "XPCOM\nTMLog01\r\n\032"

/**
 * Call NS_TraceMalloc with a valid log file descriptor to enable logging
 * of compressed malloc traces, including callsite chains.  Integers are
 * unsigned, at most 32 bits, and encoded as follows:
 *   0-127                  0xxxxxxx (binary, one byte)
 *   128-16383              10xxxxxx xxxxxxxx
 *   16384-0x1fffff         110xxxxx xxxxxxxx xxxxxxxx
 *   0x200000-0xfffffff     1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
 *   0x10000000-0xffffffff  11110000 xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
 * Strings are NUL-terminated ASCII.
 *
 * Event Operands
 *   'L' library_serial shared_object_filename_string
 *   'N' method_serial library_serial demangled_name_string
 *   'S' site_serial parent_serial method_serial calling_pc_offset
 *   'M' site_serial malloc_size
 *   'C' site_serial calloc_size
 *   'R' site_serial realloc_oldsize realloc_size
 *   'F' site_serial free_size
 *
 * See xpcom/base/bloatblame.c for an example log-file reader.
 */
PR_EXTERN(void) NS_TraceMalloc(int fd);

PR_END_EXTERN_C

#endif /* nsTraceMalloc_h___ */
