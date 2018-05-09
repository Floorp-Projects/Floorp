/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mediapacket.h"

#include <cstring>

namespace mozilla {

void
MediaPacket::Copy(const uint8_t* data, size_t len, size_t capacity)
{
  if (capacity < len) {
    capacity = len;
  }
  data_.reset(new uint8_t[capacity]);
  len_ = len;
  capacity_ = capacity;
  memcpy(data_.get(), data, len);
}

} // namespace mozilla

