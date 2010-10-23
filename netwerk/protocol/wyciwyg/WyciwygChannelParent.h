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
 *   Michal Novotny <michal.novotny@gmail.com>
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

#ifndef mozilla_net_WyciwygChannelParent_h
#define mozilla_net_WyciwygChannelParent_h

#include "mozilla/net/PWyciwygChannelParent.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsIStreamListener.h"

#include "nsIWyciwygChannel.h"

namespace mozilla {
namespace net {

class WyciwygChannelParent : public PWyciwygChannelParent
                           , public nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  WyciwygChannelParent();
  virtual ~WyciwygChannelParent();

protected:
  virtual bool RecvInit(const IPC::URI& uri);
  virtual bool RecvAsyncOpen(const IPC::URI& original,
                             const PRUint32& loadFlags);
  virtual bool RecvWriteToCacheEntry(const nsString& data);
  virtual bool RecvCloseCacheEntry(const nsresult& reason);
  virtual bool RecvSetCharsetAndSource(const PRInt32& source,
                                       const nsCString& charset);
  virtual bool RecvSetSecurityInfo(const nsCString& securityInfo);

  virtual void ActorDestroy(ActorDestroyReason why);

  nsCOMPtr<nsIWyciwygChannel> mChannel;
  bool mIPCClosed;
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_WyciwygChannelParent_h
