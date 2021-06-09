/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_extensions_ExtensionAPIAddRemoveListener_h
#define mozilla_extensions_ExtensionAPIAddRemoveListener_h

#include "ExtensionAPIRequestForwarder.h"

namespace mozilla {
namespace extensions {

class ExtensionAPIAddRemoveListener : public ExtensionAPIRequestForwarder {
 public:
  enum class EType {
    eAddListener,
    eRemoveListener,
  };

  ExtensionAPIAddRemoveListener(const EType type,
                                const nsAString& aApiNamespace,
                                const nsAString& aApiEvent,
                                const nsAString& aApiObjectType,
                                const nsAString& aApiObjectId)
      : ExtensionAPIRequestForwarder(
            type == EType::eAddListener ? APIRequestType::ADD_LISTENER
                                        : APIRequestType::REMOVE_LISTENER,
            aApiNamespace, aApiEvent, aApiObjectType, aApiObjectId) {}
};

}  // namespace extensions
}  // namespace mozilla

#endif  // mozilla_extensions_ExtensionAPIAddRemoveListener_h
