/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is  Communications Corporation..
 * Portions created by  Communications Corporation. are
 * Copyright (C) 1998  Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
