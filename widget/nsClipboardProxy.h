/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_CLIPBOARD_PROXY_H
#define NS_CLIPBOARD_PROXY_H

#include "nsIClipboard.h"
#include "mozilla/dom/PContent.h"

#define NS_CLIPBOARDPROXY_IID \
{ 0xa64c82da, 0x7326, 0x4681, \
  { 0xa0, 0x95, 0x81, 0x2c, 0xc9, 0x86, 0xe6, 0xde } }

// Hack for ContentChild to be able to know that we're an nsClipboardProxy.
class nsIClipboardProxy : public nsIClipboard
{
protected:
  typedef mozilla::dom::ClipboardCapabilities ClipboardCapabilities;

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CLIPBOARDPROXY_IID)

  virtual void SetCapabilities(const ClipboardCapabilities& aClipboardCaps) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIClipboardProxy, NS_CLIPBOARDPROXY_IID)

class nsClipboardProxy MOZ_FINAL : public nsIClipboardProxy
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARD

  nsClipboardProxy();

  virtual void SetCapabilities(const ClipboardCapabilities& aClipboardCaps) MOZ_OVERRIDE;

private:
  ~nsClipboardProxy() {}

  ClipboardCapabilities mClipboardCaps;
};

#endif
