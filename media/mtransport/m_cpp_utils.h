/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef m_cpp_utils_h__
#define m_cpp_utils_h__

#include "mozilla/Attributes.h"

namespace mozilla {

#define DISALLOW_ASSIGNMENT(T) \
  void operator=(const T& other) = delete

#define DISALLOW_COPY(T) \
  T(const T& other) = delete


#define DISALLOW_COPY_ASSIGN(T) \
  DISALLOW_COPY(T); \
  DISALLOW_ASSIGNMENT(T)

}  // close namespace
#endif
