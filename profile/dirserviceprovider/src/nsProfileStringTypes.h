/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * We support two builds of the directory service provider.
 * One, linked into the profile component, uses the internal
 * string API. The other can be used by standalone embedding
 * clients, and uses embed strings.
 * To keep the code clean, we are using typedefs to equate
 * embed/internal string types. We are also defining some
 * internal macros in terms of the embedding strings API.
 *
 * When modifying the profile directory service provider, be
 * sure to use methods supported by both the internal and
 * embed strings APIs.
 */

#ifndef MOZILLA_INTERNAL_API

#include "nsEmbedString.h"

typedef nsCString nsPromiseFlatCString;
typedef nsCString nsAutoCString;

#define PromiseFlatCString nsCString

#else
#include "nsString.h"
#include "nsPromiseFlatString.h"
#endif
