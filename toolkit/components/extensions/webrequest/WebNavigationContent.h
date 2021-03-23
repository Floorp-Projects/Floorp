/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_WebNavigationContent_h
#define mozilla_extensions_WebNavigationContent_h

#include "nsIDOMEventListener.h"
#include "nsIObserver.h"

namespace mozilla {
namespace dom {
class EventTarget;
}  // namespace dom

namespace extensions {

class WebNavigationContent final : public nsIObserver,
                                   public nsIDOMEventListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER

  static already_AddRefed<WebNavigationContent> GetSingleton();

 private:
  WebNavigationContent();
  ~WebNavigationContent() = default;

  void AttachListeners(mozilla::dom::EventTarget* aEventTarget);

  void Init();
};

}  // namespace extensions
}  // namespace mozilla

#endif  // defined mozilla_extensions_WebNavigationContent_h
