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

#define NS_FAILED(_nsresult) ((_nsresult) & 0x80000000)
#define NS_SUCCEEDED(_nsresult) (!((_nsresult) & 0x80000000))

/**
 * @name Standard return values
 */

//@{

/// Standard "it worked" return value
#define NS_OK                              0

#define NS_ERROR_BASE                      ((nsresult) 0xC1F30000)

/// Returned when an instance is not initialized
#define NS_ERROR_NOT_INITIALIZED           (NS_ERROR_BASE + 1)

/// Returned when an instance is already initialized
#define NS_ERROR_ALREADY_INITIALIZED       (NS_ERROR_BASE + 2)

/// Returned by a not implemented function
#define NS_ERROR_NOT_IMPLEMENTED           ((nsresult) 0x80004001L)

/// Returned when a given interface is not supported.
#define NS_NOINTERFACE                     ((nsresult) 0x80004002L)
#define NS_ERROR_NO_INTERFACE              NS_NOINTERFACE

#define NS_ERROR_INVALID_POINTER           ((nsresult) 0x80004003L)
#define NS_ERROR_NULL_POINTER              NS_ERROR_INVALID_POINTER

/// Returned when a function aborts
#define NS_ERROR_ABORT                     ((nsresult) 0x80004004L)

/// Returned when a function fails
#define NS_ERROR_FAILURE                   ((nsresult) 0x80004005L)

/// Returned when an unexpected error occurs
#define NS_ERROR_UNEXPECTED                ((nsresult) 0x8000ffffL)

/// Returned when a memory allocation failes
#define NS_ERROR_OUT_OF_MEMORY             ((nsresult) 0x8007000eL)

/// Returned when an illegal value is passed
#define NS_ERROR_ILLEGAL_VALUE             ((nsresult) 0x80070057L)

/// Returned when a class doesn't allow aggregation
#define NS_ERROR_NO_AGGREGATION            ((nsresult) 0x80040110L)

/// Returned when a class doesn't allow aggregation
#define NS_ERROR_NOT_AVAILABLE             ((nsresult) 0x80040111L)

/// Returned when a class is not registered
#define NS_ERROR_FACTORY_NOT_REGISTERED    ((nsresult) 0x80040154L)

/// Returned when a dynamically loaded factory couldn't be found
#define NS_ERROR_FACTORY_NOT_LOADED        ((nsresult) 0x800401f8L)

/// Returned when a factory doesn't support signatures
#define NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT \
                                           (NS_ERROR_BASE + 0x101)

/// Returned when a factory already is registered
#define NS_ERROR_FACTORY_EXISTS            (NS_ERROR_BASE + 0x100)

//@}
#endif

