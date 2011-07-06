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

#include "nsHttp.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/CookieServiceParent.h"
#include "mozilla/net/WyciwygChannelParent.h"
#include "mozilla/net/FTPChannelParent.h"
#include "mozilla/net/WebSocketChannelParent.h"
#include "mozilla/dom/TabParent.h"

#include "nsHTMLDNSPrefetch.h"

using mozilla::dom::TabParent;

namespace mozilla {
namespace net {

// C++ file contents
NeckoParent::NeckoParent()
{
}

NeckoParent::~NeckoParent()
{
}

PHttpChannelParent* 
NeckoParent::AllocPHttpChannel(PBrowserParent* browser)
{
  HttpChannelParent *p = new HttpChannelParent(browser);
  p->AddRef();
  return p;
}

bool 
NeckoParent::DeallocPHttpChannel(PHttpChannelParent* channel)
{
  HttpChannelParent *p = static_cast<HttpChannelParent *>(channel);
  p->Release();
  return true;
}

PFTPChannelParent*
NeckoParent::AllocPFTPChannel()
{
  FTPChannelParent *p = new FTPChannelParent();
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPFTPChannel(PFTPChannelParent* channel)
{
  FTPChannelParent *p = static_cast<FTPChannelParent *>(channel);
  p->Release();
  return true;
}

PCookieServiceParent* 
NeckoParent::AllocPCookieService()
{
  return new CookieServiceParent();
}

bool 
NeckoParent::DeallocPCookieService(PCookieServiceParent* cs)
{
  delete cs;
  return true;
}

PWyciwygChannelParent*
NeckoParent::AllocPWyciwygChannel()
{
  WyciwygChannelParent *p = new WyciwygChannelParent();
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPWyciwygChannel(PWyciwygChannelParent* channel)
{
  WyciwygChannelParent *p = static_cast<WyciwygChannelParent *>(channel);
  p->Release();
  return true;
}

PWebSocketParent*
NeckoParent::AllocPWebSocket(PBrowserParent* browser)
{
  TabParent* tabParent = static_cast<TabParent*>(browser);
  WebSocketChannelParent* p = new WebSocketChannelParent(tabParent);
  p->AddRef();
  return p;
}

bool
NeckoParent::DeallocPWebSocket(PWebSocketParent* actor)
{
  WebSocketChannelParent* p = static_cast<WebSocketChannelParent*>(actor);
  p->Release();
  return true;
}

bool
NeckoParent::RecvHTMLDNSPrefetch(const nsString& hostname,
                                 const PRUint16& flags)
{
  nsAutoString h(hostname);
  nsHTMLDNSPrefetch::Prefetch(h, flags);
  return true;
}

}} // mozilla::net

