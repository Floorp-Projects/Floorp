/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A utility function that converts a string to a double independent of OS locale. */

#ifndef MOZILLA_DOUBLECONVERSION_H
#define MOZILLA_DOUBLECONVERSION_H

#include "mozilla/Maybe.h"
#include "mozilla/Span.h"

#include <string>

namespace mozilla {

// Parses aStringSpan into a double floating point value. Always treats . as the
// decimal separator, regardless of OS locale. Consumes the entire string;
// trailing garbage is invalid. Returns Nothing() for invalid input.
// The implementation uses double_conversion::StringToDoubleConverter with
// NO_FLAGS, see double-conversion/string-to-double.h for more documentation.
Maybe<double> StringToDouble(Span<const char> aStringSpan);

}

#endif // MOZILLA_DOUBLECONVERSION_H
