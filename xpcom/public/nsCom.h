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
#ifdef XP_PC
#define NS_COM _declspec(dllexport)
#else  /* !XP_PC */
#define NS_COM
#endif /* !XP_PC */
#else  /* !_IMPL_NS_COM */
#ifdef XP_PC
#define NS_COM _declspec(dllimport)
#else  /* !XP_PC */
#define NS_COM
#endif /* !XP_PC */
#endif /* !_IMPL_NS_COM */

/*
 * DLL Export macro
 */

#if defined(XP_PC)

#define NS_EXPORT _declspec(dllexport)
#define NS_EXPORT_(type) _declspec(dllexport) type __stdcall

#define NS_IMETHOD_(type) virtual type __stdcall
#define NS_IMETHOD virtual nsresult __stdcall
#define NS_IMETHODIMP_(type) type __stdcall
#define NS_IMETHODIMP nsresult __stdcall

#define NS_METHOD_(type) type __stdcall
#define NS_METHOD nsresult __stdcall

#elif defined(XP_MAC)

#define NS_EXPORT __declspec(export)
#define NS_EXPORT_(type) __declspec(export) type

#define NS_IMETHOD_(type) virtual type
#define NS_IMETHOD virtual nsresult
#define NS_IMETHODIMP_(type) type
#define NS_IMETHODIMP nsresult

#define NS_METHOD_(type) type
#define NS_METHOD nsresult

#else  /* !XP_PC && !XP_MAC */

#define NS_EXPORT
#define NS_EXPORT_(type) type

#define NS_IMETHOD_(type) virtual type
#define NS_IMETHOD virtual nsresult
#define NS_IMETHODIMP_(type) type
#define NS_IMETHODIMP nsresult

#define NS_METHOD_(type) type
#define NS_METHOD nsresult

#endif /* !XP_PC */

/* use these functions to associate get/set methods with a
   C++ member variable
*/

#define NS_METHOD_GETTER(_method, _type, _member) \
_method(_type* aResult) \
{\
    if (!aResult) return NS_ERROR_NULL_POINTER; \
    *aResult = _member; \
    return NS_OK; \
}
    
#define NS_METHOD_SETTER(_method, _type, _member) \
_method(_type aResult) \
{ \
    _member = aResult; \
    return NS_OK; \
}

/* Use this inside a class declaration */
/*
 * DO NOT USE THESE IN PUBLICLY EXPORTED HEADERS 
 * If you do, the implementation may be compiled into
 * every library that #includes the header.
 */

#define NS_IMPL_CLASS_GETTER(_method, _type, _member) \
NS_IMETHOD NS_METHOD_GETTER(_method, _type, _member)

#define NS_IMPL_CLASS_SETTER(_method, _type, _member) \
NS_IMETHOD NS_METHOD_SETTER(_method, _type, _member)

#define NS_IMPL_CLASS_GETSET(_postfix, _type, _member) \
NS_IMPL_CLASS_GETTER(Get##_postfix, _type, _member) \
NS_IMPL_CLASS_SETTER(Set##_postfix, _type, _member)

/* Use these for C++ source implementation */
#define NS_IMPL_GETTER(_method, _type, _member) \
NS_IMETHODIMP NS_METHOD_GETTER(_method, _type, _member)

#define NS_IMPL_SETTER(_method, _type, _member) \
NS_IMETHODIMP NS_METHOD_SETTER(_method, _type, _member)

#define NS_IMPL_GETSET(_class, _postfix, _type, _member) \
NS_IMPL_GETTER(_class::Get##_postfix, _type, _member) \
NS_IMPL_SETTER(_class::Set##_postfix, _type, _member)

#endif


