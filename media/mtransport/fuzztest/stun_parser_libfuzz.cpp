/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "gtest/gtest.h"

#include "FuzzingInterface.h"

extern "C" {
#include <csi_platform.h>
#include "stun_msg.h"
#include "stun_codec.h"
}

int FuzzingInitStunParser(int* argc, char*** argv) { return 0; }

static int RunStunParserFuzzing(const uint8_t* data, size_t size) {
  nr_stun_message* req = 0;

  UCHAR* mes = (UCHAR*)data;

  if (!nr_stun_message_create2(&req, mes, size)) {
    nr_stun_decode_message(req, nullptr, nullptr);
    nr_stun_message_destroy(&req);
  }

  return 0;
}

MOZ_FUZZING_INTERFACE_RAW(FuzzingInitStunParser, RunStunParserFuzzing,
                          StunParser);
