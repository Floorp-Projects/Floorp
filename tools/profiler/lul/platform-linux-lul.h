/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_PLATFORM_LINUX_LUL_H
#define MOZ_PLATFORM_LINUX_LUL_H

#include "platform.h"

// Find out, in a platform-dependent way, where the code modules got
// mapped in the process' virtual address space, and get |aLUL| to
// load unwind info for them.
void
read_procmaps(lul::LUL* aLUL);

// LUL needs a callback for its logging sink.
void
logging_sink_for_LUL(const char* str);

// A singleton instance of the library.
extern lul::LUL* sLUL;

#endif /* ndef MOZ_PLATFORM_LINUX_LUL_H */
