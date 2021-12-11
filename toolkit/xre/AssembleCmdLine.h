/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AssembleCmdLine_h
#define mozilla_AssembleCmdLine_h

#if defined(XP_WIN)

#  include "mozilla/UniquePtr.h"

#  include <stdlib.h>
#  include <windows.h>

#  ifdef MOZILLA_INTERNAL_API
#    include "nsString.h"
#  endif  // MOZILLA_INTERNAL_API

namespace mozilla {

// Out param `aWideCmdLine` must be free()d by the caller.
inline int assembleCmdLine(const char* const* aArgv, wchar_t** aWideCmdLine,
                           UINT aCodePage) {
  const char* const* arg;
  char* p;
  const char* q;
  char* cmdLine;
  int cmdLineSize;
  int numBackslashes;
  int i;
  int argNeedQuotes;

  /*
   * Find out how large the command line buffer should be.
   */
  cmdLineSize = 0;
  for (arg = aArgv; *arg; ++arg) {
    /*
     * \ and " need to be escaped by a \.  In the worst case,
     * every character is a \ or ", so the string of length
     * may double.  If we quote an argument, that needs two ".
     * Finally, we need a space between arguments, and
     * a null byte at the end of command line.
     */
    cmdLineSize += 2 * strlen(*arg) /* \ and " need to be escaped */
                   + 2              /* we quote every argument */
                   + 1;             /* space in between, or final null */
  }
  p = cmdLine = (char*)malloc(cmdLineSize * sizeof(char));
  if (!p) {
    return -1;
  }

  for (arg = aArgv; *arg; ++arg) {
    /* Add a space to separates the arguments */
    if (arg != aArgv) {
      *p++ = ' ';
    }
    q = *arg;
    numBackslashes = 0;
    argNeedQuotes = 0;

    /* If the argument contains white space, it needs to be quoted. */
    if (strpbrk(*arg, " \f\n\r\t\v")) {
      argNeedQuotes = 1;
    }

    if (argNeedQuotes) {
      *p++ = '"';
    }
    while (*q) {
      if (*q == '\\') {
        numBackslashes++;
        q++;
      } else if (*q == '"') {
        if (numBackslashes) {
          /*
           * Double the backslashes since they are followed
           * by a quote
           */
          for (i = 0; i < 2 * numBackslashes; i++) {
            *p++ = '\\';
          }
          numBackslashes = 0;
        }
        /* To escape the quote */
        *p++ = '\\';
        *p++ = *q++;
      } else {
        if (numBackslashes) {
          /*
           * Backslashes are not followed by a quote, so
           * don't need to double the backslashes.
           */
          for (i = 0; i < numBackslashes; i++) {
            *p++ = '\\';
          }
          numBackslashes = 0;
        }
        *p++ = *q++;
      }
    }

    /* Now we are at the end of this argument */
    if (numBackslashes) {
      /*
       * Double the backslashes if we have a quote string
       * delimiter at the end.
       */
      if (argNeedQuotes) {
        numBackslashes *= 2;
      }
      for (i = 0; i < numBackslashes; i++) {
        *p++ = '\\';
      }
    }
    if (argNeedQuotes) {
      *p++ = '"';
    }
  }

  *p = '\0';
  int numChars = MultiByteToWideChar(aCodePage, 0, cmdLine, -1, nullptr, 0);
  *aWideCmdLine = (wchar_t*)malloc(numChars * sizeof(wchar_t));
  MultiByteToWideChar(aCodePage, 0, cmdLine, -1, *aWideCmdLine, numChars);
  free(cmdLine);
  return 0;
}

}  // namespace mozilla

#endif  // defined(XP_WIN)

#endif  // mozilla_AssembleCmdLine_h
