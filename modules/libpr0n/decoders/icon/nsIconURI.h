/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * Contributors:
 *   Scott MacGregor <mscott@netscape.com>
 */

#ifndef nsMozIconURI_h__
#define nsMozIconURI_h__

#include "nsIIconURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_MOZICONURI_CID                            \
{                                                    \
    0x43a88e0e,                                      \
    0x2d37,                                          \
    0x11d5,                                          \
    { 0x99, 0x7, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b }   \
}

class nsMozIconURI : public nsIMozIconURI
{
public:    
  NS_DECL_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIMOZICONURI

  // nsJARURI
  nsMozIconURI();
  virtual ~nsMozIconURI();

protected:
  nsCOMPtr<nsIURI> mFileIcon; // the file url we want the icon for....
  PRUint32 mSize; // the # of pixels in a row that we want for this image. Typically 16, 32, 128, etc.
  nsCString mContentType; // optional field explicitly specifying the content type
  nsCString mDummyFilePath; // if we don't have a valid file url, the file path is stored here....i.e if mFileIcon is null.....

  nsresult FormatSpec(char* *result);
};

#endif // nsMozIconURI_h__
