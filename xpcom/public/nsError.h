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

#ifndef nsError_h
#define nsError_h

#ifndef prtypes_h___
#include "prtypes.h"
#endif

/**
 * Generic result data type
 */

typedef PRUint32 nsresult;

/*
 * To add error code to your module, you need to do the following:
 *
 * 1) Add a module offset code.  Add yours to the bottom of the list 
 *    right below this comment, adding 1.
 *
 * 2) In your module, define a header file which uses one of the
 *    NE_ERROR_GENERATExxxxxx macros.  Some examples below:
 *
 *    #define NS_ERROR_MYMODULE_MYERROR1 NS_ERROR_GENERATE(NS_ERROR_SEVERITY_ERROR,NS_ERROR_MODULE_MYMODULE,1)
 *    #define NS_ERROR_MYMODULE_MYERROR2 NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_MYMODULE,2)
 *    #define NS_ERROR_MYMODULE_MYERROR3 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_MYMODULE,3)
 *
 */


/**
 * @name Standard Module Offset Code. Each Module should identify a unique number
 *       and then all errors associated with that module become offsets from the
 *       base associated with that module id. There are 16 bits of code bits for
 *       each module.
 */

#define NS_ERROR_MODULE_XPCOM      1
#define NS_ERROR_MODULE_BASE       2
#define NS_ERROR_MODULE_GFX        3
#define NS_ERROR_MODULE_WIDGET     4
#define NS_ERROR_MODULE_CALENDAR   5
#define NS_ERROR_MODULE_NETWORK    6
#define NS_ERROR_MODULE_PLUGINS    7
#define NS_ERROR_MODULE_LAYOUT     8
#define NS_ERROR_MODULE_HTMLPARSER 9
#define NS_ERROR_MODULE_RDF        10
#define NS_ERROR_MODULE_UCONV      11
#define NS_ERROR_MODULE_REG        12
#define NS_ERROR_MODULE_FILES      13

#define NS_ERROR_MODULE_MAILNEWS   16
#define NS_ERROR_MODULE_EDITOR     17

/**
 * @name Standard Error Handling Macros
 */

#define NS_FAILED(_nsresult) ((_nsresult) & 0x80000000)
#define NS_SUCCEEDED(_nsresult) (!((_nsresult) & 0x80000000))

/**
 * @name Severity Code.  This flag identifies the level of warning
 */

#define NS_ERROR_SEVERITY_SUCCESS       0
#define NS_ERROR_SEVERITY_ERROR         1

/**
 * @name Mozilla Code.  This flag separates consumers of mozilla code
 *       from the native platform
 */

#define NS_ERROR_MODULE_BASE_OFFSET 0x45

/**
 * @name Standard Error Generating Macros
 */

#define NS_ERROR_GENERATE(sev,module,code) \
    ((nsresult) (((PRUint32)(sev)<<31) | ((PRUint32)(module+NS_ERROR_MODULE_BASE_OFFSET)<<16) | ((PRUint32)(code))) )

#define NS_ERROR_GENERATE_SUCCESS(module,code) \
    ((nsresult) (((PRUint32)(NS_ERROR_SEVERITY_SUCCESS)<<31) | ((PRUint32)(module+NS_ERROR_MODULE_BASE_OFFSET)<<16) | ((PRUint32)(code))) )

#define NS_ERROR_GENERATE_FAILURE(module,code) \
    ((nsresult) (((PRUint32)(NS_ERROR_SEVERITY_ERROR)<<31) | ((PRUint32)(module+NS_ERROR_MODULE_BASE_OFFSET)<<16) | ((PRUint32)(code))) )

/**
 * @name Standard Macros for retrieving error bits
 */

#if PR_BYTES_PER_INT == 4
#define NS_IS_ERROR(err)           (((nsresult)(err))<0)
#else
#define NS_IS_ERROR(err)           (((((PRUint32)(err)) >> 31) & 0x1) == NS_ERROR_SEVERITY_ERROR)
#endif

#define NS_ERROR_GET_CODE(err)     ((err) & 0xffff)
#define NS_ERROR_GET_MODULE(err)   (((((err) >> 16) - NS_ERROR_MODULE_BASE_OFFSET) & 0x1fff))
#define NS_ERROR_GET_SEVERITY(err) (((err) >> 31) & 0x1)

/**
 * @name Standard return values
 */

/*@{*/

/* Standard "it worked" return value */
#define NS_OK                              0

/* The backwards COM false */
#define NS_COMFALSE                        1

#define NS_ERROR_BASE                      ((nsresult) 0xC1F30000)

/* Returned when an instance is not initialized */
#define NS_ERROR_NOT_INITIALIZED           (NS_ERROR_BASE + 1)

/* Returned when an instance is already initialized */
#define NS_ERROR_ALREADY_INITIALIZED       (NS_ERROR_BASE + 2)

/* Returned by a not implemented function */
#define NS_ERROR_NOT_IMPLEMENTED           ((nsresult) 0x80004001L)

/* Returned when a given interface is not supported. */
#define NS_NOINTERFACE                     ((nsresult) 0x80004002L)
#define NS_ERROR_NO_INTERFACE              NS_NOINTERFACE

#define NS_ERROR_INVALID_POINTER           ((nsresult) 0x80004003L)
#define NS_ERROR_NULL_POINTER              NS_ERROR_INVALID_POINTER

/* Returned when a function aborts */
#define NS_ERROR_ABORT                     ((nsresult) 0x80004004L)

/* Returned when a function fails */
#define NS_ERROR_FAILURE                   ((nsresult) 0x80004005L)

/* Returned when an unexpected error occurs */
#define NS_ERROR_UNEXPECTED                ((nsresult) 0x8000ffffL)

/* Returned when a memory allocation failes */
#define NS_ERROR_OUT_OF_MEMORY             ((nsresult) 0x8007000eL)

/* Returned when an illegal value is passed */
#define NS_ERROR_ILLEGAL_VALUE             ((nsresult) 0x80070057L)
#define NS_ERROR_INVALID_ARG               NS_ERROR_ILLEGAL_VALUE

/* Returned when a class doesn't allow aggregation */
#define NS_ERROR_NO_AGGREGATION            ((nsresult) 0x80040110L)

/* Returned when a class doesn't allow aggregation */
#define NS_ERROR_NOT_AVAILABLE             ((nsresult) 0x80040111L)

/* Returned when a class is not registered */
#define NS_ERROR_FACTORY_NOT_REGISTERED    ((nsresult) 0x80040154L)

/* Returned when a dynamically loaded factory couldn't be found */
#define NS_ERROR_FACTORY_NOT_LOADED        ((nsresult) 0x800401f8L)

/* Returned when a factory doesn't support signatures */
#define NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT \
                                           (NS_ERROR_BASE + 0x101)

/* Returned when a factory already is registered */
#define NS_ERROR_FACTORY_EXISTS            (NS_ERROR_BASE + 0x100)

/*@}*/

////////////////////////////////////////////////////////////////////////////////

#ifdef XP_PC
#pragma warning(disable: 4251) // 'nsCOMPtr<class nsIInputStream>' needs to have dll-interface to be used by clients of class 'nsInputStream'
#pragma warning(disable: 4275) // non dll-interface class 'nsISupports' used as base for dll-interface class 'nsIRDFNode'
#endif

#endif

