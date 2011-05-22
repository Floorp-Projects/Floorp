/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Boris Zbarsky <bzbarsky@mit.edu>  (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUnicharStreamLoader_h__
#define nsUnicharStreamLoader_h__

#include "nsIChannel.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIUnicodeDecoder.h"
#include "nsCOMPtr.h"
#include "nsString.h"

class nsIInputStream;

class nsUnicharStreamLoader : public nsIUnicharStreamLoader
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARSTREAMLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsUnicharStreamLoader() {}
  virtual ~nsUnicharStreamLoader() {}

  static nsresult Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  nsresult DetermineCharset();

  /**
   * callback method used for ReadSegments
   */
  static NS_METHOD WriteSegmentFun(nsIInputStream *, void *, const char *,
                                   PRUint32, PRUint32, PRUint32 *);

  nsCOMPtr<nsIUnicharStreamLoaderObserver> mObserver;
  nsCOMPtr<nsIUnicodeDecoder>              mDecoder;
  nsCOMPtr<nsISupports>                    mContext;
  nsCOMPtr<nsIChannel>                     mChannel;
  nsCString                                mCharset;

  // This holds the first up-to-512 bytes of the raw stream.
  // It will be passed to the OnDetermineCharset callback.
  nsCString                                mRawData;

  // This holds the complete contents of the stream so far, after
  // decoding to UTF-16.  It will be passed to the OnStreamComplete
  // callback.
  nsString                                 mBuffer;
};

#endif // nsUnicharStreamLoader_h__
