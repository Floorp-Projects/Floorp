/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsIAtom.h"
#include "nsParserService.h"
#include "nsElementTable.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"

nsParserService::nsParserService()
{
}

nsParserService::~nsParserService()
{
}

NS_IMPL_ISUPPORTS(nsParserService, nsIParserService)

int32_t
nsParserService::HTMLAtomTagToId(nsIAtom* aAtom) const
{
  return nsHTMLTags::LookupTag(nsDependentAtomString(aAtom));
}

int32_t
nsParserService::HTMLCaseSensitiveAtomTagToId(nsIAtom* aAtom) const
{
  return nsHTMLTags::CaseSensitiveLookupTag(aAtom);
}

int32_t
nsParserService::HTMLStringTagToId(const nsAString& aTag) const
{
  return nsHTMLTags::LookupTag(aTag);
}

NS_IMETHODIMP
nsParserService::IsContainer(int32_t aId, bool& aIsContainer) const
{
  aIsContainer = nsHTMLElement::IsContainer((eHTMLTags)aId);

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::IsBlock(int32_t aId, bool& aIsBlock) const
{
  aIsBlock = nsHTMLElement::IsBlock((eHTMLTags)aId);

  return NS_OK;
}
