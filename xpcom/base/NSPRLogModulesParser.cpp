/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSPRLogModulesParser.h"

#include "mozilla/Tokenizer.h"

const char kDelimiters[] = ", ";
const char kAdditionalWordChars[] = "_-";

namespace mozilla {

void
NSPRLogModulesParser(const char* aLogModules,
                     function<void(const char*, LogLevel, int32_t)> aCallback)
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
    int32_t levelValue = 0;
    if (parser.CheckChar(':')) {
      // Check if a negative value is provided.
      int32_t multiplier = 1;
      if (parser.CheckChar([](const char aChar) { return aChar == '-'; })) {
        multiplier = -1;
      }

      // NB: If a level isn't provided after the ':' we assume the default
      //     Error level is desired. This differs from NSPR which will stop
      //     processing the log module string in this case.
      if (parser.ReadInteger(&levelValue)) {
        logLevel = ToLogLevel(levelValue * multiplier);
      }
    }

    aCallback(moduleName.get(), logLevel, levelValue);

    // Skip ahead to the next token.
    parser.SkipWhites();
  }
}

} // namespace mozilla
