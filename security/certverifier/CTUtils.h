/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTUtils_h
#define CTUtils_h

#include "pkix/Input.h"
#include "pkix/Result.h"

namespace mozilla { namespace ct {

// Reads a TLS-encoded variable length unsigned integer from |in|.
// The integer is expected to be in big-endian order, which is used by TLS.
// Note: checks if the output parameter overflows while reading.
// |length| indicates the size (in bytes) of the serialized integer.
template <size_t length, typename T>
pkix::Result ReadUint(Reader& in, T& out);

// Reads a length-prefixed variable amount of bytes from |in|, updating |out|
// on success. |prefixLength| indicates the number of bytes needed to represent
// the length.
template <size_t prefixLength>
pkix::Result ReadVariableBytes(Reader& in, Input& out);

} } // namespace mozilla::ct

#endif //CTUtils_h
