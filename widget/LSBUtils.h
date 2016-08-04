/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_WIDGET_LSB_UTILS_H
#define _MOZILLA_WIDGET_LSB_UTILS_H

#include "nsString.h"

namespace mozilla {
namespace widget {
namespace lsb {

// Fetches the LSB release data by parsing the lsb_release command.
// Returns false if the lsb_release command was not found, or parsing failed.
bool GetLSBRelease(nsACString& aDistributor,
                   nsACString& aDescription,
                   nsACString& aRelease,
                   nsACString& aCodename);


}  // namespace lsb
}  // namespace widget
}  // namespace mozilla

#endif // _MOZILLA_WIDGET_LSB_UTILS_H
