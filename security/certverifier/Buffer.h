/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Buffer_h
#define Buffer_h

#include "mozilla/Vector.h"

namespace mozilla { namespace ct {

typedef Vector<uint8_t> Buffer;

} } // namespace mozilla::ct

namespace mozilla {

// Comparison operators are placed under mozilla namespace since
// mozilla::ct::Buffer is actually mozilla::Vector.
bool operator==(const ct::Buffer& a, const ct::Buffer& b);
bool operator!=(const ct::Buffer& a, const ct::Buffer& b);

} // namespace mozilla


#endif // Buffer_h
