/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LogCommandLineHandler.h"

#include "mozilla/Tokenizer.h"
#include "nsDebug.h"

namespace mozilla {

void LoggingHandleCommandLineArgs(int argc, char const* const* argv,
                                  std::function<void(nsACString const&)> const& consumer)
{
  // Keeps the name of a pending env var (MOZ_LOG or MOZ_LOG_FILE) that
  // we expect to get a value for in the next iterated arg.
  // Used for the `-MOZ_LOG <modules>` form of argument.
  nsAutoCString env;

  auto const names = {
    NS_LITERAL_CSTRING("MOZ_LOG"),
    NS_LITERAL_CSTRING("MOZ_LOG_FILE")
  };

  for (int arg = 1; arg < argc; ++arg) {
    Tokenizer p(argv[arg]);

    if (!env.IsEmpty() && p.CheckChar('-')) {
      NS_WARNING("Expects value after -MOZ_LOG(_FILE) argument, but another argument found");

      // We only expect values for the pending env var, start over
      p.Rollback();
      env.Truncate();
    }

    if (env.IsEmpty()) {
      if (!p.CheckChar('-')) {
        continue;
      }
      // We accept `-MOZ_LOG` as well as `--MOZ_LOG`.
      Unused << p.CheckChar('-');

      for (auto const& name : names) {
        if (!p.CheckWord(name)) {
          continue;
        }

        env.Assign(name);
        env.Append('=');
        break;
      }

      if (env.IsEmpty()) {
        // An unknonwn argument, ignore.
        continue;
      }

      // We accept `-MOZ_LOG <modules>` as well as `-MOZ_LOG=<modules>`.

      if (p.CheckEOF()) {
        // We have a lone `-MOZ_LOG` arg, the next arg is expected to be
        // the value, |env| is now prepared as `MOZ_LOG=`.
        continue;
      }

      if (!p.CheckChar('=')) {
        // There is a character after the arg name and it's not '=',
        // ignore this argument.
        NS_WARNING("-MOZ_LOG(_FILE) argument not in a proper form");

        env.Truncate();
        continue;
      }
    }

    // This can be non-empty from previous iteration or in this iteration.
    if (!env.IsEmpty()) {
      nsDependentCSubstring value;
      Unused << p.ReadUntil(Tokenizer::Token::EndOfFile(), value);
      env.Append(value);

      consumer(env);

      env.Truncate();
    }
  }
}

} // mozilla