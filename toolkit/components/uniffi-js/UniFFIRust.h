/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_UniFFIRust_h
#define mozilla_UniFFIRust_h

#include <stdint.h>

namespace mozilla::uniffi {

// Low-level Rust structs for UniFFI

// RustCallStatus.code values
constexpr int8_t RUST_CALL_SUCCESS = 0;
constexpr int8_t RUST_CALL_ERROR = 1;
constexpr int8_t RUST_CALL_INTERNAL_ERROR = 2;

// structs/functions from UniFFI
extern "C" {
struct RustBuffer {
  int32_t capacity;
  int32_t len;
  uint8_t* data;
};

struct RustCallStatus {
  int8_t code;
  RustBuffer error_buf;
};

RustBuffer uniffi_rustbuffer_alloc(int32_t size, RustCallStatus* call_status);
void uniffi_rustbuffer_free(RustBuffer buf, RustCallStatus* call_status);
}

}  // namespace mozilla::uniffi

#endif /* mozilla_UniFFIRust_h */
