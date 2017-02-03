/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Services_h
#define mozilla_Services_h

#include "nscore.h"
#include "nsCOMPtr.h"

#define MOZ_USE_NAMESPACE
#define MOZ_SERVICE(NAME, TYPE, SERVICE_CID) class TYPE;

#include "ServiceList.h"
#undef MOZ_SERVICE
#undef MOZ_USE_NAMESPACE

namespace mozilla {
namespace services {

#ifdef MOZILLA_INTERNAL_API
#define MOZ_SERVICE(NAME, TYPE, SERVICE_CID)                        \
    already_AddRefed<TYPE> Get##NAME();

#include "ServiceList.h"
#undef MOZ_SERVICE
#endif

} // namespace services
} // namespace mozilla

#endif
