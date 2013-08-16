/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CTOKEN__
#define CTOKEN__

#include "prtypes.h"
#include "nsString.h"
#include "nsError.h"

class nsScanner;
class nsTokenAllocator;

enum eHTMLTokenTypes {
  eToken_unknown=0,
  eToken_start=1,      eToken_end,          eToken_comment,         eToken_entity,
  eToken_whitespace,   eToken_newline,      eToken_text,            eToken_attribute,
  eToken_instruction,  eToken_cdatasection, eToken_doctypeDecl,     eToken_markupDecl,
  eToken_last //make sure this stays the last token...
};

#endif


