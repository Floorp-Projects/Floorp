/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

extern "C" {

  void MozillaRegisterDebugFD(int fd) {}
  void MozillaRegisterDebugFILE(FILE *f) {}
  void MozillaUnRegisterDebugFD(int fd) {}
  void MozillaUnRegisterDebugFILE(FILE *f) {}

}