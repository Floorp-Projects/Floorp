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

//
// Mike Pinkerton
// Netscape Communications
//
// See header file for details
//


#include "nsMimeMapper.h"

#include "nsString.h"
#include "nsIDataFlavor.h"

//
// MapMimeTypeToMacOSType
//
// Given a mime type, map this into the appropriate MacOS clipboard type. For
// types that we don't know about intrinsicly, use a hash to get a unique 4
// character code.
//
ResType
nsMimeMapperMac :: MapMimeTypeToMacOSType ( const nsString & aMimeStr )
{
  ResType format = 0;

  if (aMimeStr.Equals(kTextMime) )
    format = 'TEXT';
  else if ( aMimeStr.Equals(kXIFMime) )
    format = 'XIF ';
  else if ( aMimeStr.Equals(kHTMLMime) )
    format = 'HTML';
  else
    format = '????';   //еее Need to use a hash here
    
  /*
   else if (aMimeStr.Equals(kUnicodeMime)) {
    format = CF_UNICODETEXT;
  } else if (aMimeStr.Equals(kJPEGImageMime)) {
    format = CF_BITMAP;
  } else {
    char * str = aMimeStr.ToNewCString();
    format = ::RegisterClipboardFormat(str);
    delete[] str;
  }
  */
  
  return format;
  
} // MapMimeTypeToMacOSType
