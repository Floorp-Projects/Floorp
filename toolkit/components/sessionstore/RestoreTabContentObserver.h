/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RestoreTabContentObserver_h
#define mozilla_dom_RestoreTabContentObserver_h

#include "mozilla/StaticPtr.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

// An observer for restoring session store data at a correct time.
class RestoreTabContentObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static void Initialize();
  static void Shutdown();

 private:
  RestoreTabContentObserver() = default;

  virtual ~RestoreTabContentObserver() = default;
  static mozilla::StaticRefPtr<RestoreTabContentObserver>
      gRestoreTabContentObserver;
};
}  // namespace dom
}  // namespace mozilla
#endif  // mozilla_dom_RestoreTabContentObserver_h
