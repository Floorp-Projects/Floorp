/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/extensions/ExtensionsParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace extensions {

void ExtensionsParent::ActorDestroy(ActorDestroyReason aWhy) {}

}  // namespace extensions
}  // namespace mozilla
