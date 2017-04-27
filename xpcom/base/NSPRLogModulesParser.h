/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include <functional>

namespace mozilla {

/**
 * Helper function that parses the legacy NSPR_LOG_MODULES env var format
 * for specifying log levels and logging options.
 *
 * @param aLogModules The log modules configuration string.
 * @param aCallback The callback to invoke for each log module config entry.
 */
void NSPRLogModulesParser(const char* aLogModules,
                          const std::function<void(const char*, LogLevel, int32_t)>& aCallback);

} // namespace mozilla
