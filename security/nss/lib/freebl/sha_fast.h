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
 * The Original Code is SHA 180-1 Reference Implementation (Optimized)
 * 
 * The Initial Developer of the Original Code is Paul Kocher of
 * Cryptography Research.  Portions created by Paul Kocher are 
 * Copyright (C) 1995-9 by Cryptography Research, Inc.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *
 *     Paul Kocher
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

#ifndef _SHA_FAST_H_
#define _SHA_FAST_H_

#define SHA1_INPUT_LEN 64

struct SHA1ContextStr {
  union {
    PRUint32 w[80];		/* input buffer, plus 64 words */
    PRUint8  b[320];
  } u;
  PRUint32 H[5];		/* 5 state variables */
  PRUint32 sizeHi,sizeLo;	/* 64-bit count of hashed bytes. */
};

#define SHA_MASK      0x00FF00FF
#if defined(IS_LITTLE_ENDIAN)
#define SHA_HTONL(x)  (A = (x), A = A << 16 | A >> 16, \
                       (A & SHA_MASK) << 8 | (A >> 8) & SHA_MASK)
#else
#define SHA_HTONL(x)  (x)
#endif
#define SHA_BYTESWAP(x) x = SHA_HTONL(x)

#endif /* _SHA_FAST_H_ */
