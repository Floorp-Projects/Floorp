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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Duell <jduell.mcbugs@gmail.com>
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

#include "mozilla/net/PNeckoParent.h"
#include "mozilla/net/NeckoCommon.h"

#ifndef mozilla_net_NeckoParent_h
#define mozilla_net_NeckoParent_h

namespace mozilla {
namespace net {

// Header file contents
class NeckoParent :
  public PNeckoParent
{
public:
  NeckoParent();
  virtual ~NeckoParent();

protected:
  virtual PHttpChannelParent* AllocPHttpChannel(PBrowserParent* browser);
  virtual bool DeallocPHttpChannel(PHttpChannelParent*);
  virtual PCookieServiceParent* AllocPCookieService();
  virtual bool DeallocPCookieService(PCookieServiceParent*);
  virtual PWyciwygChannelParent* AllocPWyciwygChannel();
  virtual bool DeallocPWyciwygChannel(PWyciwygChannelParent*);
  virtual PFTPChannelParent* AllocPFTPChannel();
  virtual bool DeallocPFTPChannel(PFTPChannelParent*);
  virtual PWebSocketParent* AllocPWebSocket(PBrowserParent* browser);
  virtual bool DeallocPWebSocket(PWebSocketParent*);
  virtual bool RecvHTMLDNSPrefetch(const nsString& hostname,
                                   const PRUint16& flags);
};

} // namespace net
} // namespace mozilla

#endif // mozilla_net_NeckoParent_h
