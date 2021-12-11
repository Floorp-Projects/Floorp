/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPICallSyncFunction_h
#define mozilla_extensions_ExtensionAPICallSyncFunction_h

#include "ExtensionAPIRequestForwarder.h"

namespace mozilla {
namespace extensions {

class ExtensionAPICallSyncFunction : public ExtensionAPIRequestForwarder {
 public:
  ExtensionAPICallSyncFunction(const nsAString& aApiNamespace,
                               const nsAString& aApiMethod,
                               const nsAString& aApiObjectType = u""_ns,
                               const nsAString& aApiObjectId = u""_ns)
      : ExtensionAPIRequestForwarder(
            mozIExtensionAPIRequest::RequestType::CALL_FUNCTION, aApiNamespace,
            aApiMethod, aApiObjectType, aApiObjectId) {}
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPICallSyncFunction_h
