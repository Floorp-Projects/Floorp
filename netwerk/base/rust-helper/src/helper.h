/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RUST_NS_NET_HELPER
#define RUST_NS_NET_HELPER

#include "nsError.h"
#include "nsString.h"

extern "C" {

nsresult rust_prepare_accept_languages(const nsACString* i_accept_languages,
                                       nsACString* o_accept_languages);

bool rust_net_is_valid_ipv4_addr(const nsACString& aAddr);

bool rust_net_is_valid_ipv6_addr(const nsACString& aAddr);
}

#endif  // RUST_NS_NET_HELPER
