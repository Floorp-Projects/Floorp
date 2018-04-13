/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LogCommandLineHandler_h
#define LogCommandLineHandler_h

#include <functional>
#include "nsString.h"

namespace mozilla {

/**
 * A helper function parsing provided command line arguments and handling two
 * specific args:
 *
 * -MOZ_LOG=modulelist
 * -MOZ_LOG_FILE=file/path
 *
 * both expecting an argument, and corresponding to the same-name environment
 * variables we use for logging setup.
 *
 * When an argument is found in the proper form, the consumer callback is called
 * with a string in a follwing form, note that we do this for every occurence,
 * and not just at the end of the parsing:
 *
 * "MOZ_LOG=modulelist" or "MOZ_LOG_FILE=file/path"
 *
 * All the following forms of arguments of the application are possible:
 *
 * --MOZ_LOG modulelist
 * -MOZ_LOG modulelist
 * --MOZ_LOG=modulelist
 * -MOZ_LOG=modulelist
 *
 * The motivation for a separte function and not implementing a command line
 * handler interface is that we need to process this very early during the
 * application startup.  Command line handlers are proccessed way later
 * after logging has already been set up.
 */
void
LoggingHandleCommandLineArgs(int argc, char const* const* argv,
                             std::function<void(nsACString const&)> const& consumer);

} // mozilla

#endif
