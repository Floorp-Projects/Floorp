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

#ifndef nsIDataFlavor_h__
#define nsIDataFlavor_h__

#include "nsISupports.h"
#include "nsString.h"


// {8B5314BD-DB01-11d2-96CE-0060B0FB9956}
#define NS_IDATAFLAVOR_IID      \
{ 0x8b5314bd, 0xdb01, 0x11d2, { 0x96, 0xce, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

//const char * kTextMime    = "text/text";
//const char * kUnicodeMime = "text/unicode";
//const char * kHTMLMime    = "text/html";
//const char * kAOLMailMime = "AOLMAIL";
//const char * kImageMime   = "text/image";

#define kTextMime      "text/txt"
#define kXIFMime       "text/xif"
#define kUnicodeMime   "text/unicode"
#define kHTMLMime      "text/html"
#define kAOLMailMime   "AOLMAIL"
#define kPNGImageMime  "image/png"
#define kJPEGImageMime "image/jpg"
#define kGIFImageMime  "image/gif"

class nsIDataFlavor : public nsISupports {

  public:

  /**
    * Initializes the data flavor 
    *
    * @param  aMimeType mime string
    * @param  aHumanPresentableName human readable string for mime
    */
    NS_IMETHOD Init(const nsString & aMimeType, const nsString & aHumanPresentableName) = 0;

  /**
    * Gets the Mime string 
    *
    * @param  aMimeStr string to be set
    */
    NS_IMETHOD GetMimeType(nsString & aMimeStr) = 0;

  /**
    * Gets the Human readable version of the mime string 
    *
    * @param  aReadableStr string to be set
    */
    NS_IMETHOD GetHumanPresentableName(nsString & aReadableStr) = 0;


};

#endif
