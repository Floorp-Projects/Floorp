/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSPRLogModulesParser.h"

#include "mozilla/Tokenizer.h"

const char kDelimiters[] = ", ";
const char kAdditionalWordChars[] = "_-";
const char* kReservedNames[] = {
  "all",
  "append",
  "bufsize",
  "sync",
  "timestamp",
};

namespace mozilla {

void
NSPRLogModulesParser(const char* aLogModules,
                     Function<void(const char*, LogLevel)> aCallback)
{
  if (!aLogModules) {
    return;
  }

  Tokenizer parser(aLogModules, kDelimiters, kAdditionalWordChars);
  nsAutoCString moduleName;

  // Format: LOG_MODULES="Foo:2,Bar, Baz:5"
  while (parser.ReadWord(moduleName)) {
    // Next should be :<level>, default to Error if not provided.
    LogLevel logLevel = LogLevel::Error;
    if (parser.CheckChar(':')) {
      // Check if a negative value is provided.
      int32_t multiplier = 1;
      if (parser.CheckChar([](const char aChar) { return aChar == '-'; })) {
        multiplier = -1;
      }

      // NB: If a level isn't provided after the ':' we assume the default
      //     Error level is desired. This differs from NSPR which will stop
      //     processing the log module string in this case.
      int32_t level;
      if (parser.ReadInteger(&level)) {
        logLevel = ToLogLevel(level * multiplier);
      }
    }

    // NSPR reserves a few modules names for logging options. We just skip
    // those entries here.
    bool isReserved = false;
    for (size_t i = 0; i < PR_ARRAY_SIZE(kReservedNames); i++) {
      if (moduleName.EqualsASCII(kReservedNames[i])) {
        isReserved = true;
        break;
      }
    }

    if (!isReserved) {
      aCallback(moduleName.get(), logLevel);
    }

    // Skip ahead to the next token.
    parser.SkipWhites();
  }
}

} // namespace mozilla
