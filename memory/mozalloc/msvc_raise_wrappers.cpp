/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exception>
#include "mozalloc_abort.h"

static void __cdecl
RaiseHandler(const std::exception& e)
{
    mozalloc_abort(e.what());
}

static struct StaticScopeStruct final {
    StaticScopeStruct() {
        std::exception::_Set_raise_handler(RaiseHandler);
    }
} StaticScopeInvoke;
