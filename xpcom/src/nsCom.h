/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsCom_h__
#define nsCom_h__

/*
 * API Import/Export macros
 */

#ifdef _IMPL_NS_COM
#  ifdef XP_PC
#    define NS_COM _declspec(dllexport)
#  else  // !XP_PC
#    define NS_COM
#  endif // !XP_PC
#else  // !_IMPL_NS_COM
#  ifdef XP_PC
#    define NS_COM _declspec(dllimport)
#  else  // !XP_PC
#    define NS_COM
#  endif // !XP_PC
#endif // !_IMPL_NS_COM

/*
 * DLL Export macro
 */

#ifndef NS_EXPORT
#  ifdef XP_PC
#    define NS_EXPORT           _declspec(dllexport)
#  else  // !XP_PC
#    define NS_EXPORT
#  endif // !XP_PC
#endif // !NS_EXPORT

#ifdef XP_PC
#  define NS_METHOD_(Type)      Type __stdcall
#  define NS_METHOD             nsresult __stdcall
#else  // !XP_PC
#  define NS_METHOD_(Type)      Type
#  define NS_METHOD             nsresult
#endif // !XP_PC

#define NS_IMETHOD_(Type)       virtual NS_METHOD_(Type)
#define NS_IMETHOD              virtual NS_METHOD

#endif


