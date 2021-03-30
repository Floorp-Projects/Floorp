/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_OpaqueResponseUtils_h
#define mozilla_net_OpaqueResponseUtils_h

namespace mozilla {
namespace net {

bool IsOpaqueSafeListedMIMEType(const nsACString& aContentType);

bool IsOpaqueBlockListedMIMEType(const nsACString& aContentType);

bool IsOpaqueBlockListedNeverSniffedMIMEType(const nsACString& aContentType);

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_OpaqueResponseUtils_h
