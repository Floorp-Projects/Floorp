/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIStringStream_h___
#define nsIStringStream_h___

#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsString.h"

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewStringInputStream(
    nsISupports** aStreamResult,
    const nsString& aStringToRead);
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCStringInputStream(
    nsISupports** aStreamResult,
    const nsCString& aStringToRead);
    // Factory method to get an nsInputStream from a cstring.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewStringOutputStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange);
    // Factory method to get an nsOutputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCharInputStream(
    nsISupports** aStreamResult,
    const char* aStringToRead);
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCharOutputStream(
    nsISupports** aStreamResult,
    char** aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewStringIOStream(
    nsISupports** aStreamResult,
    nsString& aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
// file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewCharIOStream(
    nsISupports** aStreamResult,
    char** aStringToChange);
    // Factory method to get an nsOutputStream to a string.  Result will implement all the
    // file stream interfaces in nsIFileStream.h

//----------------------------------------------------------------------------------------
extern "C" NS_COM nsresult NS_NewByteInputStream(
    nsISupports** aStreamResult,
    const char* aStringToRead,
    PRInt32 aLength);
    // Factory method to get an nsInputStream from a string.  Result will implement all the
    // 
#endif /* nsIStringStream_h___ */
