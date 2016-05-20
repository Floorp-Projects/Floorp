/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NS_PARSERSERVICE_H__
#define NS_PARSERSERVICE_H__

#include "nsIParserService.h"

extern "C" int MOZ_XMLIsLetter(const char* ptr);
extern "C" int MOZ_XMLIsNCNameChar(const char* ptr);
/**
 * Decodes an entity into the UTF-16 encoding of a Unicode character. If a ';'
 * is found between `ptr` and `end` it will try to decode the entity and set
 * `*next` to point to the character after the ;. The resulting UTF-16 code
 * units will be written in `*result`, so if the entity is a valid numeric
 * entity there needs to be space for at least two char16_t at the location
 * `result` points to.
 *
 * @param ptr pointer to the ampersand.
 * @param end pointer to the position after the last character of the
 *            string.
 * @param next [out] will be set to the character after the ';' or null if
 *                   the decoding was unsuccessful.
 * @param result the buffer to write the resulting UTF-16 character in.
 * @return the number of char16_t written to `*result`.
 */
extern "C" int MOZ_XMLTranslateEntity(const char* ptr, const char* end,
                                      const char** next, char16_t* result);

class nsParserService : public nsIParserService {
  virtual ~nsParserService();

public:
  nsParserService();

  NS_DECL_ISUPPORTS

  int32_t HTMLAtomTagToId(nsIAtom* aAtom) const override;

  int32_t HTMLCaseSensitiveAtomTagToId(nsIAtom* aAtom) const override;

  int32_t HTMLStringTagToId(const nsAString& aTag) const override;

  const char16_t *HTMLIdToStringTag(int32_t aId) const override;
  
  nsIAtom *HTMLIdToAtomTag(int32_t aId) const override;

  NS_IMETHOD HTMLConvertEntityToUnicode(const nsAString& aEntity, 
                                        int32_t* aUnicode) const override;
  NS_IMETHOD HTMLConvertUnicodeToEntity(int32_t aUnicode,
                                        nsCString& aEntity) const override;
  NS_IMETHOD IsContainer(int32_t aId, bool& aIsContainer) const override;
  NS_IMETHOD IsBlock(int32_t aId, bool& aIsBlock) const override;
};

#endif
