/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SECURITY_SANDBOX_SHIM_BASE_STRING_PIECE_H
#define _SECURITY_SANDBOX_SHIM_BASE_STRING_PIECE_H
#include "sandbox/base/strings/string_piece.h"

namespace {
bool IsStringASCII(const base::StringPiece& ascii)
{
  return IsStringASCII(ascii.as_string());
}
}
#endif
