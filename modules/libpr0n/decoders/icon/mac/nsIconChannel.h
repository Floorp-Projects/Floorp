/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * Contributor(s): 
 *  Scott MacGregor <mscott@netscape.com>
 */

#ifndef nsIconChannel_h___
#define nsIconChannel_h___

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURI.h"
#include <Icons.h>

class nsIFile;

class nsIconChannel : public nsIChannel
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHANNEL

  nsIconChannel();
  virtual ~nsIconChannel();

  nsresult Init(nsIURI* uri);

protected:
  nsCOMPtr<nsIURI> mUrl;
  nsCOMPtr<nsIURI> mOriginalURI;
  PRUint32         mLoadAttributes;
  PRInt32          mContentLength;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsISupports>  mOwner; 
  nsresult mStatus;
  
  nsresult ExtractIconInfoFromUrl(nsIFile ** aLocalFile, PRUint32 * aDesiredImageSize,
                           nsACString &aContentType, nsACString &aFileExtension);

  nsresult GetLockedIconData(IconFamilyHandle iconFamilyH, PRUint32 iconType,
                           Handle iconDataH, PRUint32 *iconDataSize);

  nsresult GetLockedIcons(IconFamilyHandle iconFamily, PRUint32 desiredImageSize,
                           Handle iconH, PRUint32 *dataCount, PRBool *isIndexedData,
                           Handle iconMaskH, PRUint32 *maskCount);

};

#endif /* nsIconChannel_h___ */
