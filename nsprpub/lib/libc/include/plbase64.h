/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#ifndef _plbase64_h
#define _plbase64_h

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/*
 * PL_Base64Encode
 *
 * This routine encodes the data pointed to by the "src" parameter using the
 * base64 algorithm, and returns a pointer to the result.  If the "srclen"
 * parameter is not zero, it specifies the length of the source data.  If it
 * is zero, the source data is assumed to be null-terminated, and PL_strlen
 * is used to determine the source length.  If the "dest" parameter is not
 * null, it is assumed to point to a buffer of sufficient size (which may be
 * calculated: ((srclen + 2)/3)*4) into which the encoded data is placed 
 * (without any termination).  If the "dest" parameter is null, a buffer is
 * allocated from the heap to hold the encoded data, and the result *will*
 * be terminated with an extra null character.  It is the caller's 
 * responsibility to free the result when it is allocated.  A null is returned 
 * if the allocation fails.
 */

PR_EXTERN(char *)
PL_Base64Encode
(
    const char *src,
    PRUint32    srclen,
    char       *dest
);

/*
 * PL_Base64Decode
 *
 * This routine decodes the data pointed to by the "src" parameter using
 * the base64 algorithm, and returns a pointer to the result.  The source
 * may either include or exclude any trailing '=' characters.  If the
 * "srclen" parameter is not zero, it specifies the length of the source
 * data.  If it is zero, PL_strlen will be used to determine the source
 * length.  If the "dest" parameter is not null, it is assumed to point to
 * a buffer of sufficient size (which may be calculated: (srclen * 3)/4
 * when srclen includes the '=' characters) into which the decoded data
 * is placed (without any termination).  If the "dest" parameter is null,
 * a buffer is allocated from the heap to hold the decoded data, and the
 * result *will* be terminated with an extra null character.  It is the
 * caller's responsibility to free the result when it is allocated.  A null
 * is retuned if the allocation fails, or if the source is not well-coded.
 */

PR_EXTERN(char *)
PL_Base64Decode
(
    const char *src,
    PRUint32    srclen,
    char       *dest
);

PR_END_EXTERN_C

#endif /* _plbase64_h */
