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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
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
/*
 * Copyright (C) 1994-1999 RSA Security Inc. Licence to copy this document
 * is granted provided that it is identified as "RSA Security In.c Public-Key
 * Cryptography Standards (PKCS)" in all material mentioning or referencing
 * this document.
 */
/* Define API */
#ifndef _FPKCS11_H_
#define _FPKCS11_H_ 1

#include "seccomon.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* All the various pkcs11 types and #define'd values are in the file */
/* pkcs11t.h.  CK_PTR should be defined there, too; it's the recipe for */
/* making pointers. */
#include "fpkcs11t.h"

#define __PASTE(x,y)	x##y

/* ================================================================= */
/* Define the "extern" form of all the entry points */

#define CK_EXTERN	extern
#define CK_FUNC(name)	CK_ENTRY name
#define CK_NEED_ARG_LIST	1
#define _CK_RV		PR_PUBLIC_API(CK_RV)

/* pkcs11f.h has all the information about the PKCS #11 functions. */
#include "fpkcs11f.h"

#undef CK_FUNC
#undef CK_EXTERN
#undef CK_NEED_ARG_LIST
#undef _CK_RV

/* ================================================================= */
/* Define the typedef form of all the entry points. */
/* That is, for each Cryptoki function C_XXX, define a type CK_C_XXX */
/* which is a pointer to that kind of function. */

#define CK_EXTERN               typedef
#define CK_FUNC(name)           CK_ENTRY (CK_PTR __PASTE(CK_,name))
#define CK_NEED_ARG_LIST	1
#define _CK_RV		        CK_RV

#include "fpkcs11f.h"

#undef CK_FUNC
#undef CK_EXTERN
#undef CK_NEED_ARG_LIST
#undef _CK_RV

/* =================================================================
 * Define structed vector of entry points.
 * The CK_FUNCTION_LIST contains a CK_VERSION indicating the PKCS #11
 * version, and then a whole slew of function pointers to the routines
 * in the library.  This type was declared, but not defined, in
 * pkcs11t.h. */


/* These data types are platform/implementation dependent. */
#if defined(XP_WIN) 
#if defined(_WIN32)
#define CK_ENTRY	
#define CK_PTR		*	/* definition for Win32 */ 
#define NULL_PTR 	0	/* NULL pointer */
#pragma pack(push, cryptoki, 1)
#else /* win16 */
#if defined(__WATCOMC__)
#define CK_ENTRY	
#define CK_PTR		*	/* definition for Win16 */ 
#define NULL_PTR 	0	/* NULL pointer */
#pragma pack(push, 1)
#else /* not Watcom 16-bit */
#define CK_ENTRY	
#define CK_PTR		*	/* definition for Win16 */ 
#define NULL_PTR 	0	/* NULL pointer */
#pragma pack(1)
#endif
#endif
#else /* not windows */
#define CK_ENTRY
#define CK_PTR		*	/* definition for UNIX */ 
#define NULL_PTR 	0	/* NULL pointer */
#endif


#define CK_EXTERN 
#define CK_FUNC(name) __PASTE(CK_,name) name;
#define _CK_RV

struct CK_FUNCTION_LIST {

    CK_VERSION		version;	/* PKCS #11 version */

/* Pile all the function pointers into it. */
#include "fpkcs11f.h"

};

#undef CK_FUNC
#undef CK_EXTERN
#undef _CK_RV


#if defined(XP_WIN)
#if defined(_WIN32)
#pragma pack(pop, cryptoki)
#else /* win16 */
#if defined(__WATCOMC__)
#pragma pack(pop)
#else /* not Watcom 16-bit */
#pragma pack()
#endif
#endif
#endif


#undef __PASTE
/* ================================================================= */

#ifdef __cplusplus
}
#endif

#endif
