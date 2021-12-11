/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utilities to pull in OpenSSL-formatted keys.

#ifndef nsskeys_h_
#define nsskeys_h_

#include "cert.h"
#include "keyhi.h"

#include <string>

SECKEYPrivateKey* ReadPrivateKey(const std::string& file);
CERTCertificate* ReadCertificate(const std::string& file);

#endif
