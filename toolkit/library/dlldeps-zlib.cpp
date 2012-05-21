/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Force references to all of the symbols that we want exported from
// the dll that are located in the .lib files we link with

#define ZLIB_INTERNAL
#include "zlib.h"

void xxxNeverCalledZLib()
{
    deflate(0, 0);
    deflateInit(0, 0);
	deflateInit2(0, 0, 0, 0, 0, 0);
    deflateEnd(0);
    inflate(0, 0);
    inflateInit(0);
    inflateInit2(0, 0);
    inflateEnd(0);
    inflateReset(0);
}
