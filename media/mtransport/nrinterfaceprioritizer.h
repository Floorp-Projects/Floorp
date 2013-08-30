/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nrinterfacepriority_h__
#define nrinterfacepriority_h__

extern "C" {
#include "nr_api.h"
#include "nr_interface_prioritizer.h"
}

namespace mozilla {

nr_interface_prioritizer* CreateInterfacePrioritizer();

} // namespace mozilla

#endif
