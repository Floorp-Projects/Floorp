/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef telemetry_UntrustedModules_h__
#define telemetry_UntrustedModules_h__

#include "jsapi.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace Telemetry {

/**
 * This function returns a promise that asynchronously processes and gathers
 * untrusted module data. The promise is either resolved with the JS object
 * ping payload, or is rejected upon failure.
 */
nsresult GetUntrustedModuleLoadEvents(JSContext* cx, dom::Promise** aPromise);

}  // namespace Telemetry
}  // namespace mozilla

#endif  // telemetry_UntrustedModules_h__
