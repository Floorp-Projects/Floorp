/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// libav uses an autoconf created config file for compile time definitions.
// Since we're only using a tiny part of the library, the configuration for
// common platforms have been pregenerated, and should be enough for our
// needs.
//
// The platform specific config files contain header existence information and
// things like prefixing requirements. The common header has processor
// architecture definitions and project compilation directives.

#ifndef MOZ_LIBAV_CONFIG_H
#define MOZ_LIBAV_CONFIG_H
#if defined(XP_WIN)
#include "config_win.h"
#elif defined(XP_DARWIN)
#include "config_darwin.h"
#elif defined(XP_UNIX)
#include "config_unix.h"
#endif
#include "config_common.h"
#endif
