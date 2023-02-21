/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LINK_SERVICE_COMMON_H_
#define LINK_SERVICE_COMMON_H_

namespace mozilla {
class SHA1Sum;
}

// Add a seed to the computed network ID to prevent user linkability.
void SeedNetworkId(mozilla::SHA1Sum& aSha1);

#endif  // LINK_SERVICE_COMMON_H_
