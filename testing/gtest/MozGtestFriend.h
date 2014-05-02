/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTEST_MOZGTESTFRIEND_H
#define GTEST_MOZGTESTFRIEND_H

#ifdef ENABLE_TESTS
#include "gtest_prod.h"
#else
#define FRIEND_TEST(a, b)
#endif

#endif // GTEST_MOZGTESTFRIEND_H
