/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PARSER_HTML_RLBOX_EXPAT_TYPES_H_
#define PARSER_HTML_RLBOX_EXPAT_TYPES_H_

#include <stddef.h>
#include "mozilla/rlbox/rlbox_types.hpp"

#ifdef MOZ_WASM_SANDBOXING_EXPAT
namespace rlbox {
class rlbox_wasm2c_sandbox;
}
using rlbox_expat_sandbox_type = rlbox::rlbox_wasm2c_sandbox;
#else
using rlbox_expat_sandbox_type = rlbox::rlbox_noop_sandbox;
#endif

using rlbox_sandbox_expat = rlbox::rlbox_sandbox<rlbox_expat_sandbox_type>;
template <typename T>
using sandbox_callback_expat =
    rlbox::sandbox_callback<T, rlbox_expat_sandbox_type>;
template <typename T>
using tainted_expat = rlbox::tainted<T, rlbox_expat_sandbox_type>;
template <typename T>
using tainted_opaque_expat = rlbox::tainted_opaque<T, rlbox_expat_sandbox_type>;
template <typename T>
using tainted_volatile_expat =
    rlbox::tainted_volatile<T, rlbox_expat_sandbox_type>;
using rlbox::tainted_boolean_hint;
template <typename T>
using app_pointer_expat = rlbox::app_pointer<T, rlbox_expat_sandbox_type>;

#endif
