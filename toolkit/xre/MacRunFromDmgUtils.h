/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file defines the interface between Cocoa-specific Obj-C++ and generic
// C++, so it itself cannot have any Obj-C bits in it.

#ifndef MacRunFromDmgUtils_h_
#define MacRunFromDmgUtils_h_

namespace mozilla {
namespace MacRunFromDmgUtils {

/**
 * Returns true if the app is running from the read-only filesystem of a
 * mounted .dmg.  Returns false if not, or if we fail to determine whether it
 * is.
 */
bool IsAppRunningFromDmg();

}  // namespace MacRunFromDmgUtils
}  // namespace mozilla

#endif
