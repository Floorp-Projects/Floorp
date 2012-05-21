/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NS_PARSERSERVICE_H__
#define NS_PARSERSERVICE_H__

#include "nsIParserService.h"
#include "nsDTDUtils.h"

extern "C" int MOZ_XMLIsLetter(const char* ptr);
extern "C" int MOZ_XMLIsNCNameChar(const char* ptr);
extern "C" int MOZ_XMLTranslateEntity(const char* ptr, const char* end,
                                      const char** next, PRUnichar* result);

class nsParserService : public nsIParserService {
public:
  nsParserService();
  virtual ~nsParserService();

  NS_DECL_ISUPPORTS

  PRInt32 HTMLAtomTagToId(nsIAtom* aAtom) const;

  PRInt32 HTMLCaseSensitiveAtomTagToId(nsIAtom* aAtom) const;

  PRInt32 HTMLStringTagToId(const nsAString& aTag) const;

  const PRUnichar *HTMLIdToStringTag(PRInt32 aId) const;
  
  nsIAtom *HTMLIdToAtomTag(PRInt32 aId) const;

  NS_IMETHOD HTMLConvertEntityToUnicode(const nsAString& aEntity, 
                                        PRInt32* aUnicode) const;
  NS_IMETHOD HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                        nsCString& aEntity) const;
  NS_IMETHOD IsContainer(PRInt32 aId, bool& aIsContainer) const;
  NS_IMETHOD IsBlock(PRInt32 aId, bool& aIsBlock) const;
};

#endif
