// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#define WHITESPACE_UNICODE \
  0x0009, /* <control-0009> to <control-000D> */ \
  0x000A,                                        \
  0x000B,                                        \
  0x000C,                                        \
  0x000D,                                        \
  0x0020, /* Space */                            \
  0x0085, /* <control-0085> */                   \
  0x00A0, /* No-Break Space */                   \
  0x1680, /* Ogham Space Mark */                 \
  0x180E, /* Mongolian Vowel Separator */        \
  0x2000, /* En Quad to Hair Space */            \
  0x2001,                                        \
  0x2002,                                        \
  0x2003,                                        \
  0x2004,                                        \
  0x2005,                                        \
  0x2006,                                        \
  0x2007,                                        \
  0x2008,                                        \
  0x2009,                                        \
  0x200A,                                        \
  0x200C, /* Zero Width Non-Joiner */            \
  0x2028, /* Line Separator */                   \
  0x2029, /* Paragraph Separator */              \
  0x202F, /* Narrow No-Break Space */            \
  0x205F, /* Medium Mathematical Space */        \
  0x3000, /* Ideographic Space */                \
  0

const wchar_t kWhitespaceWide[] = {
  WHITESPACE_UNICODE
};

const char16 kWhitespaceUTF16[] = {
  WHITESPACE_UNICODE
};

const char kWhitespaceASCII[] = {
  0x09,    // <control-0009> to <control-000D>
  0x0A,
  0x0B,
  0x0C,
  0x0D,
  0x20,    // Space
  0
};

const char kUtf8ByteOrderMark[] = "\xEF\xBB\xBF";
