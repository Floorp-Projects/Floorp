/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnterpriseRoots_h
#define EnterpriseRoots_h

#include "mozilla/Vector.h"

nsresult GatherEnterpriseRoots(
    mozilla::Vector<mozilla::Vector<uint8_t>>& roots);

#endif  // EnterpriseRoots_h
