/*
 * Copyright (c) 2008-2010 Mozilla Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef nsHtml5NamedCharacters_h
#define nsHtml5NamedCharacters_h

#include "jArray.h"
#include "nscore.h"
#include "nsDebug.h"
#include "mozilla/Logging.h"
#include "nsMemory.h"

struct nsHtml5CharacterName {
  uint16_t nameStart;
  uint16_t nameLen;
#ifdef DEBUG
  int32_t n;
#endif
  int32_t length() const;
  char16_t charAt(int32_t index) const;
};

class nsHtml5NamedCharacters {
 public:
  static const nsHtml5CharacterName NAMES[];
  static const char16_t VALUES[][2];
  static char16_t** WINDOWS_1252;
  static void initializeStatics();
  static void releaseStatics();
};

#endif  // nsHtml5NamedCharacters_h
