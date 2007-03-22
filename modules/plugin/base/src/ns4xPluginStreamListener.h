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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef ns4xPluginStreamListener_h__
#define ns4xPluginStreamListener_h__

#include "nsIPluginStreamListener.h"
#include "nsIPluginStreamInfo.h"
#include "nsIHTTPHeaderListener.h"
#include "nsIRequest.h"
#include "nsITimer.h"
#include "nsCOMPtr.h"

#define MAX_PLUGIN_NECKO_BUFFER 16384

class ns4xPluginInstance;
class nsI4xPluginStreamInfo;

class ns4xPluginStreamListener : public nsIPluginStreamListener,
                                 public nsITimerCallback,
                                 public nsIHTTPHeaderListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGINSTREAMLISTENER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIHTTPHEADERLISTENER

  // ns4xPluginStreamListener specific methods:
  ns4xPluginStreamListener(nsIPluginInstance* inst, void* notifyData,
                           const char* aURL);
  virtual ~ns4xPluginStreamListener();
  PRBool IsStarted();
  nsresult CleanUpStream(NPReason reason);
  void CallURLNotify(NPReason reason);
  void SetCallNotify(PRBool aCallNotify)
  {
    mCallNotify = aCallNotify;
  }
  nsresult SuspendRequest();
  void ResumeRequest();
  nsresult StartDataPump();
  void StopDataPump();

protected:
  void* mNotifyData;
  char* mStreamBuffer;
  char* mNotifyURL;
  ns4xPluginInstance* mInst;
  NPStream mNPStream;
  PRUint32 mStreamBufferSize;
  PRInt32 mStreamBufferByteCount;
  nsPluginStreamType mStreamType;
  PRPackedBool mStreamStarted;
  PRPackedBool mStreamCleanedUp;
  PRPackedBool mCallNotify;
  PRPackedBool mIsSuspended;
  nsCString mResponseHeaders;
  char* mResponseHeaderBuf;

  nsCOMPtr<nsITimer> mDataPumpTimer;

public:
  nsCOMPtr<nsIPluginStreamInfo> mStreamInfo;
};

// nsI4xPluginStreamInfo is an internal helper interface that exposes
// the underlying necko request to consumers of nsIPluginStreamInfo's.
#define NS_I4XPLUGINSTREAMINFO_IID       \
{ 0x097fdaaa, 0xa2a3, 0x49c2, \
  {0x91, 0xee, 0xeb, 0xc5, 0x7d, 0x6c, 0x9c, 0x97} }

class nsI4xPluginStreamInfo : public nsIPluginStreamInfo
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_I4XPLUGINSTREAMINFO_IID)

  nsIRequest *GetRequest()
  {
    return mRequest;
  }

protected:
  nsCOMPtr<nsIRequest> mRequest;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsI4xPluginStreamInfo,
                              NS_I4XPLUGINSTREAMINFO_IID)

#endif
