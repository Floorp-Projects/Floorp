/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsParserUtils_h
#define nsParserUtils_h

#include "nsIScriptableUnescapeHTML.h"
#include "nsIParserUtils.h"
#include "mozilla/Attributes.h"

class nsParserUtils MOZ_FINAL : public nsIScriptableUnescapeHTML,
                                public nsIParserUtils
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTABLEUNESCAPEHTML
  NS_DECL_NSIPARSERUTILS
};

#endif // nsParserUtils_h
