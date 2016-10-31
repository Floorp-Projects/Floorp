/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef shared_h__
#define shared_h__

#include "nss.h"

class NSSDatabase {
 public:
  NSSDatabase() { NSS_NoDB_Init(nullptr); }
  ~NSSDatabase() { NSS_Shutdown(); }
};

#endif  // shared_h__
