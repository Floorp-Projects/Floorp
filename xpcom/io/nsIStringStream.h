/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIStringStream_h___
#define nsIStringStream_h___

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsString.h"

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringInputStream(
    nsISupports** aStreamResult,
    const nsString& aStringToRead);
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringOutputStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange);
    // Factory method to get an nsOutputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewCharInputStream(
    nsISupports** aStreamResult,
    const char* aStringToRead);
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewCharOutputStream(
    nsISupports** aStreamResult,
    char** aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewStringIOStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
// file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_BASE nsresult NS_NewCharIOStream(
    nsISupports** aStreamResult,
    char** aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

#endif /* nsIStringStream_h___ */
