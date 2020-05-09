/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSPRLogModulesParser.h"

#include "mozilla/Tokenizer.h"

const char kDelimiters[] = ", ";
const char kAdditionalWordChars[] = "_-.";

namespace mozilla {

void NSPRLogModulesParser(
    const char* aLogModules,
    const std::function<void(const char*, LogLevel, int32_t)>& aCallback) {
  if (!aLogModules) {
    return;
  }

  Tokenizer parser(aLogModules, kDelimiters, kAdditionalWordChars);
  nsAutoCString moduleName;

  Tokenizer::Token rustModSep =
      parser.AddCustomToken("::", Tokenizer::CASE_SENSITIVE);

  auto readModuleName = [&](nsAutoCString& moduleName) -> bool {
    moduleName.Truncate();
    nsDependentCSubstring sub;
    parser.Record();
    // If the name doesn't include at least one word, we've reached the end.
    if (!parser.ReadWord(sub)) {
      return false;
    }
    // We will exit this loop if when any of the condition fails
    while (parser.Check(rustModSep) && parser.ReadWord(sub)) {
    }
    // Claim will include characters of the last sucessfully read item
    parser.Claim(moduleName, Tokenizer::INCLUDE_LAST);
    return true;
  };

  // Format: LOG_MODULES="Foo:2,Bar, Baz:5,rust_crate::mod::file:5"
  while (readModuleName(moduleName)) {
    // Next should be :<level>, default to Error if not provided.
    LogLevel logLevel = LogLevel::Error;
    int32_t levelValue = 0;
    if (parser.CheckChar(':')) {
      // NB: If a level isn't provided after the ':' we assume the default
      //     Error level is desired. This differs from NSPR which will stop
      //     processing the log module string in this case.
      if (parser.ReadSignedInteger(&levelValue)) {
        logLevel = ToLogLevel(levelValue);
      }
    }

    aCallback(moduleName.get(), logLevel, levelValue);

    // Skip ahead to the next token.
    parser.SkipWhites();
  }
}

}  // namespace mozilla
