/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_toolkit_system_windowsproxy_ProxyUtils_h
#define mozilla_toolkit_system_windowsproxy_ProxyUtils_h

#include "nsString.h"

namespace mozilla {
namespace toolkit {
namespace system {

bool IsHostProxyEntry(const nsACString& aHost, const nsACString& aOverride);

} // namespace system
} // namespace toolkit
} // namespace mozilla

#endif // mozilla_toolkit_system_windowsproxy_ProxyUtils_h
