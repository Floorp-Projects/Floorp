/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SHA 180-1 Reference Implementation (Optimized).
 *
 * The Initial Developer of the Original Code is
 * Paul Kocher of Cryptography Research.
 * Portions created by the Initial Developer are Copyright (C) 1995-9
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _SHA_FAST_H_
#define _SHA_FAST_H_

#include "prlong.h"

#define SHA1_INPUT_LEN 64

#if defined(IS_64) && !defined(__sparc) 
typedef PRUint64 SHA_HW_t;
#define SHA1_USING_64_BIT 1
#else
typedef PRUint32 SHA_HW_t;
#endif

struct SHA1ContextStr {
  union {
    PRUint32 w[16];		/* input buffer */
    PRUint8  b[64];
  } u;
  PRUint64 size;          	/* count of hashed bytes. */
  SHA_HW_t H[22];		/* 5 state variables, 16 tmp values, 1 extra */
};

#if defined(_MSC_VER)
#include <stdlib.h>
#if defined(IS_LITTLE_ENDIAN) 
#if (_MSC_VER >= 1300)
#pragma intrinsic(_byteswap_ulong)
#define SHA_HTONL(x) _byteswap_ulong(x)
#elif defined(NSS_X86_OR_X64)
#ifndef FORCEINLINE
#if (_MSC_VER >= 1200)
#define FORCEINLINE __forceinline
#else
#define FORCEINLINE __inline
#endif /* _MSC_VER */
#endif /* !defined FORCEINLINE */
#define FASTCALL __fastcall

static FORCEINLINE PRUint32 FASTCALL 
swap4b(PRUint32 dwd) 
{
    __asm {
    	mov   eax,dwd
	bswap eax
    }
}

#define SHA_HTONL(x) swap4b(x)
#endif /* NSS_X86_OR_X64 */
#endif /* IS_LITTLE_ENDIAN */

#pragma intrinsic (_lrotr, _lrotl) 
#define SHA_ROTL(x,n) _lrotl(x,n)
#define SHA_ROTL_IS_DEFINED 1
#endif /* _MSC_VER */

#if defined(__GNUC__) 
/* __x86_64__  and __x86_64 are defined by GCC on x86_64 CPUs */
#if defined( SHA1_USING_64_BIT )
static __inline__ PRUint64 SHA_ROTL(PRUint64 x, PRUint32 n)
{
    PRUint32 t = (PRUint32)x;
    return ((t << n) | (t >> (32 - n)));
}
#else 
static __inline__ PRUint32 SHA_ROTL(PRUint32 t, PRUint32 n)
{
    return ((t << n) | (t >> (32 - n)));
}
#endif
#define SHA_ROTL_IS_DEFINED 1

#if defined(NSS_X86_OR_X64)
static __inline__ PRUint32 swap4b(PRUint32 value)
{
    __asm__("bswap %0" : "+r" (value));
    return (value);
}
#define SHA_HTONL(x) swap4b(x)

#elif defined(__thumb2__) || \
      (!defined(__thumb__) && \
       (defined(__ARM_ARCH_6__) || \
        defined(__ARM_ARCH_6J__) || \
        defined(__ARM_ARCH_6K__) || \
        defined(__ARM_ARCH_6Z__) || \
        defined(__ARM_ARCH_6ZK__) || \
        defined(__ARM_ARCH_6T2__) || \
        defined(__ARM_ARCH_7__) || \
        defined(__ARM_ARCH_7A__) || \
        defined(__ARM_ARCH_7R__)))
static __inline__ PRUint32 swap4b(PRUint32 value)
{
    PRUint32 ret;
    __asm__("rev %0, %1" : "=r" (ret) : "r"(value));
    return ret;
}
#define SHA_HTONL(x) swap4b(x)

#endif /* x86 family */

#endif /* __GNUC__ */

#if !defined(SHA_ROTL_IS_DEFINED)
#define SHA_NEED_TMP_VARIABLE 1
#define SHA_ROTL(X,n) (tmp = (X), ((tmp) << (n)) | ((tmp) >> (32-(n))))
#endif

#if defined(NSS_X86_OR_X64)
#define SHA_ALLOW_UNALIGNED_ACCESS 1
#endif

#if !defined(SHA_HTONL)
#define SHA_MASK      0x00FF00FF
#if defined(IS_LITTLE_ENDIAN)
#undef  SHA_NEED_TMP_VARIABLE 
#define SHA_NEED_TMP_VARIABLE 1
#define SHA_HTONL(x)  (tmp = (x), tmp = (tmp << 16) | (tmp >> 16), \
                       ((tmp & SHA_MASK) << 8) | ((tmp >> 8) & SHA_MASK))
#else
#define SHA_HTONL(x)  (x)
#endif
#endif

#define SHA_BYTESWAP(x) x = SHA_HTONL(x)

#define SHA_STORE(n) ((PRUint32*)hashout)[n] = SHA_HTONL(ctx->H[n])
#if defined(SHA_ALLOW_UNALIGNED_ACCESS)
#define SHA_STORE_RESULT \
  SHA_STORE(0); \
  SHA_STORE(1); \
  SHA_STORE(2); \
  SHA_STORE(3); \
  SHA_STORE(4);

#elif defined(IS_LITTLE_ENDIAN) || defined( SHA1_USING_64_BIT )
#define SHA_STORE_RESULT \
  if (!((ptrdiff_t)hashout % sizeof(PRUint32))) { \
    SHA_STORE(0); \
    SHA_STORE(1); \
    SHA_STORE(2); \
    SHA_STORE(3); \
    SHA_STORE(4); \
  } else { \
    ctx->u.w[0] = SHA_HTONL(ctx->H[0]); \
    ctx->u.w[1] = SHA_HTONL(ctx->H[1]); \
    ctx->u.w[2] = SHA_HTONL(ctx->H[2]); \
    ctx->u.w[3] = SHA_HTONL(ctx->H[3]); \
    ctx->u.w[4] = SHA_HTONL(ctx->H[4]); \
    memcpy(hashout, ctx->u.w, SHA1_LENGTH); \
  }

#else
#define SHA_STORE_RESULT \
  if (!((ptrdiff_t)hashout % sizeof(PRUint32))) { \
    SHA_STORE(0); \
    SHA_STORE(1); \
    SHA_STORE(2); \
    SHA_STORE(3); \
    SHA_STORE(4); \
  } else { \
    memcpy(hashout, ctx->H, SHA1_LENGTH); \
  }
#endif 

#endif /* _SHA_FAST_H_ */
