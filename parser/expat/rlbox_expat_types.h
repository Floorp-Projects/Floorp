/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PARSER_HTML_RLBOX_EXPAT_TYPES_H_
#define PARSER_HTML_RLBOX_EXPAT_TYPES_H_

#include <stddef.h>
#include "mozilla/rlbox/rlbox_types.hpp"

#ifdef MOZ_WASM_SANDBOXING_EXPAT
RLBOX_DEFINE_BASE_TYPES_FOR(expat, wasm2c)
#else
RLBOX_DEFINE_BASE_TYPES_FOR(expat, noop)
#endif

#endif
