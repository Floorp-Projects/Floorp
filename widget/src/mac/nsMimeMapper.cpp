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
#include "nsITransferable.h"
#include "nsString.h"

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
  else if ( aMimeStr.Equals("moz/toolbaritem") )    //¥¥¥ hack until the hash implemented
    format = 'TITM';
  else if ( aMimeStr.Equals("moz/toolbar") )        //¥¥¥ hack until the hash implemented
    format = 'TBAR';
  else {
    NS_NOTYETIMPLEMENTED("Unsupported mime type, Pink needs to write a hash routine");
    format = '????';   //¥¥¥ Need to use a hash here
  }
  
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


//
// MapMacOSTypeToMimeType
//
// Given a MacOS flavor, map this back into the Mozilla mimetype. 
//
void
nsMimeMapperMac :: MapMacOSTypeToMimeType ( ResType inMacType, nsString & outMimeStr )
{
  switch ( inMacType ) {
  
    case 'TEXT': outMimeStr = kTextMime; break;
    case 'XIF ': outMimeStr = kXIFMime; break;
    case 'HTML': outMimeStr = kHTMLMime; break;
    case 'TITM': outMimeStr = "moz/toolbaritem"; break;   //¥¥¥Êhack until un-hash implemented
    case 'TBAR': outMimeStr = "moz/toolbar"; break;       //¥¥¥Êhack until un-hash implemented
   
    default:
      outMimeStr = "unknown";
      NS_NOTYETIMPLEMENTED("Unsupported mime type, Pink needs to write a unhash routine");
      //¥¥¥ need to un-hash here.
  
  } // case of which flavor

} // MapMacOSTypeToMimeType
