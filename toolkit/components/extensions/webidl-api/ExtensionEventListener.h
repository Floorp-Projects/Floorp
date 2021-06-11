/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionEventListener_h
#define mozilla_extensions_ExtensionEventListener_h

#include "mozIExtensionAPIRequestHandling.h"

namespace mozilla {
namespace extensions {

// A class that represents a callback parameter (passed to addListener /
// removeListener), instances of this class are received by the
// mozIExtensionAPIRequestHandler as a property of the mozIExtensionAPIRequest.
// The mozIExtensionEventListener xpcom interface provides methods that allow
// the mozIExtensionAPIRequestHandler running in the Main Thread to call the
// underlying callback Function on its owning thread.
class ExtensionEventListener : public mozIExtensionEventListener {
 public:
  NS_DECL_MOZIEXTENSIONEVENTLISTENER
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionEventListener_h
