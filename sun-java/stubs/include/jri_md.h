/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef JRI_MD_H
#define JRI_MD_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(XP_PC) || defined(_WINDOWS) || defined(WIN32) || defined(_WIN32)
#	include <windows.h>
#	if defined(_MSC_VER)
#		if defined(WIN32) || defined(_WIN32)
#			define JRI_PUBLIC_API(ResultType)	_declspec(dllexport) ResultType
#			define JRI_PUBLIC_VAR(VarType)		VarType
#		else /* !_WIN32 */
#		    if defined(_WINDLL)
#			define JRI_PUBLIC_API(ResultType)	ResultType __cdecl __export __loadds 
#			define JRI_PUBLIC_VAR(VarType)		VarType
#		else /* !WINDLL */
#			define JRI_PUBLIC_API(ResultType)	ResultType __cdecl __export
#			define JRI_PUBLIC_VAR(VarType)		VarType
#                   endif /* !WINDLL */
#		endif /* !_WIN32 */
#	elif defined(__BORLANDC__)
#		if defined(WIN32) || defined(_WIN32)
#			define JRI_PUBLIC_API(ResultType)	__export ResultType
#			define JRI_PUBLIC_VAR(VarType)		VarType
#		else /* !_WIN32 */
#			define JRI_PUBLIC_API(ResultType)	ResultType _cdecl _export _loadds 
#			define JRI_PUBLIC_VAR(VarType)		VarType
#		endif
#	else
#		error Unsupported PC development environment.	
#	endif
#	ifndef IS_LITTLE_ENDIAN
#		define IS_LITTLE_ENDIAN
#	endif

/* Mac */
#elif macintosh || Macintosh || THINK_C
#	if defined(__MWERKS__)				/* Metrowerks */
#		if !__option(enumsalwaysint)
#			error You need to define 'Enums Always Int' for your project.
#		endif
#		if defined(GENERATING68K) && !GENERATINGCFM 
#			if !__option(fourbyteints) 
#				error You need to define 'Struct Alignment: 68k' for your project.
#			endif
#		endif /* !GENERATINGCFM */
#		define JRI_PUBLIC_API(ResultType)	__declspec(export) ResultType
#		define JRI_PUBLIC_VAR(VarType)		JRI_PUBLIC_API(VarType)
#	elif defined(__SC__)				/* Symantec */
#		error What are the Symantec defines? (warren@netscape.com)
#	elif macintosh && applec			/* MPW */
#		error Please upgrade to the latest MPW compiler (SC).
#	else
#		error Unsupported Mac development environment.
#	endif

/* Unix or else */
#else
#   define JRI_PUBLIC_API(ResultType)	    ResultType
#   define JRI_PUBLIC_VAR(VarType)          VarType
#endif

typedef unsigned char	jbool;
typedef char		jbyte;
#ifdef IS_64 /* XXX ok for alpha, but not right on all 64-bit architectures */
typedef int		jint;
#else
typedef long		jint;
#endif

#ifdef __cplusplus
}
#endif
#endif /* JRI_MD_H */
