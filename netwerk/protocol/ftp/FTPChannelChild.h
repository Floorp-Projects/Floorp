/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

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
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alon Zakai <azakai@mozilla.com>
 *   Josh Matthews <josh@joshmatthews.net>
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

#ifndef mozilla_net_FTPChannelChild_h
#define mozilla_net_FTPChannelChild_h

#include "mozilla/net/PFTPChannelChild.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "nsBaseChannel.h"
#include "nsIFTPChannel.h"
#include "nsIUploadChannel.h"
#include "nsIProxiedChannel.h"
#include "nsIResumableChannel.h"
#include "nsIChildChannel.h"

#include "nsIStreamListener.h"

namespace mozilla {
namespace net {

// This class inherits logic from nsBaseChannel that is not needed for an
// e10s child channel, but it works.  At some point we could slice up
// nsBaseChannel and have a new class that has only the common logic for
// nsFTPChannel/FTPChannelChild.

class FTPChannelChild : public PFTPChannelChild
                      , public nsBaseChannel
                      , public nsIFTPChannel
                      , public nsIUploadChannel
                      , public nsIResumableChannel
                      , public nsIProxiedChannel
                      , public nsIChildChannel
{
public:
  typedef ::nsIStreamListener nsIStreamListener;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFTPCHANNEL
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIRESUMABLECHANNEL
  NS_DECL_NSIPROXIEDCHANNEL
  NS_DECL_NSICHILDCHANNEL

  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend();
  NS_IMETHOD Resume();

  FTPChannelChild(nsIURI* uri);
  virtual ~FTPChannelChild();

  void AddIPDLReference();
  void ReleaseIPDLReference();

  NS_IMETHOD AsyncOpen(nsIStreamListener* listener, nsISupports* aContext);

  // Note that we handle this ourselves, overriding the nsBaseChannel
  // default behavior, in order to be e10s-friendly.
  NS_IMETHOD IsPending(bool* result);

  NS_OVERRIDE nsresult OpenContentStream(bool async,
                                         nsIInputStream** stream,
                                         nsIChannel** channel);

  bool IsSuspended();

protected:
  NS_OVERRIDE bool RecvOnStartRequest(const PRInt32& aContentLength,
                                      const nsCString& aContentType,
                                      const PRTime& aLastModified,
                                      const nsCString& aEntityID,
                                      const IPC::URI& aURI);
  NS_OVERRIDE bool RecvOnDataAvailable(const nsCString& data,
                                       const PRUint32& offset,
                                       const PRUint32& count);
  NS_OVERRIDE bool RecvOnStopRequest(const nsresult& statusCode);
  NS_OVERRIDE bool RecvCancelEarly(const nsresult& statusCode);
  NS_OVERRIDE bool RecvDeleteSelf();

  void DoOnStartRequest(const PRInt32& aContentLength,
                        const nsCString& aContentType,
                        const PRTime& aLastModified,
                        const nsCString& aEntityID,
                        const IPC::URI& aURI);
  void DoOnDataAvailable(const nsCString& data,
                         const PRUint32& offset,
                         const PRUint32& count);
  void DoOnStopRequest(const nsresult& statusCode);
  void DoCancelEarly(const nsresult& statusCode);
  void DoDeleteSelf();

  friend class FTPStartRequestEvent;
  friend class FTPDataAvailableEvent;
  friend class FTPStopRequestEvent;
  friend class FTPCancelEarlyEvent;
  friend class FTPDeleteSelfEvent;

private:
  nsCOMPtr<nsIInputStream> mUploadStream;

  bool mIPCOpen;
  ChannelEventQueue mEventQ;
  bool mCanceled;
  PRUint32 mSuspendCount;
  bool mIsPending;
  bool mWasOpened;
  
  PRTime mLastModifiedTime;
  PRUint64 mStartPos;
  nsCString mEntityID;
};

inline bool
FTPChannelChild::IsSuspended()
{
  return mSuspendCount != 0;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_FTPChannelChild_h
