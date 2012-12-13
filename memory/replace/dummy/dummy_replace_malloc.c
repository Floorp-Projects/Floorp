/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Types.h"

/*
 * Dummy functions for linking purpose on OSX with older XCode.
 * See details in configure.in, under "Replace-malloc Mac linkage quirks"
 */
#define MALLOC_FUNCS MALLOC_FUNCS_ALL
#define MALLOC_DECL(name, ...) \
  MOZ_EXPORT void replace_ ## name() { }

#include "malloc_decls.h"
