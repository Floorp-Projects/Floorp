/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef util_h__
#define util_h__

#include <cassert>
#include <vector>

std::vector<uint8_t> hex_string_to_bytes(std::string s) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < s.length(); i += 2) {
    bytes.push_back(std::stoul(s.substr(i, 2), nullptr, 16));
  }
  return bytes;
}

#endif  // util_h__
