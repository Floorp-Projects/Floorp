/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMError.h"
#include "nsIAtom.h"
#include "nsParserService.h"
#include "nsHTMLEntities.h"
#include "nsElementTable.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"

nsParserService::nsParserService()
{
}

nsParserService::~nsParserService()
{
}

NS_IMPL_ISUPPORTS1(nsParserService, nsIParserService)

PRInt32
nsParserService::HTMLAtomTagToId(nsIAtom* aAtom) const
{
  return nsHTMLTags::LookupTag(nsDependentAtomString(aAtom));
}

PRInt32
nsParserService::HTMLCaseSensitiveAtomTagToId(nsIAtom* aAtom) const
{
  return nsHTMLTags::CaseSensitiveLookupTag(aAtom);
}

PRInt32
nsParserService::HTMLStringTagToId(const nsAString& aTag) const
{
  return nsHTMLTags::LookupTag(aTag);
}

const PRUnichar*
nsParserService::HTMLIdToStringTag(PRInt32 aId) const
{
  return nsHTMLTags::GetStringValue((nsHTMLTag)aId);
}
  
nsIAtom*
nsParserService::HTMLIdToAtomTag(PRInt32 aId) const
{
  return nsHTMLTags::GetAtom((nsHTMLTag)aId);
}

NS_IMETHODIMP
nsParserService::HTMLConvertEntityToUnicode(const nsAString& aEntity,
                                            PRInt32* aUnicode) const
{
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                            nsCString& aEntity) const
{
  const char* str = nsHTMLEntities::UnicodeToEntity(aUnicode);
  if (str) {
    aEntity.Assign(str);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::IsContainer(PRInt32 aId, bool& aIsContainer) const
{
  aIsContainer = nsHTMLElement::IsContainer((eHTMLTags)aId);

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::IsBlock(PRInt32 aId, bool& aIsBlock) const
{
  if((aId>eHTMLTag_unknown) && (aId<eHTMLTag_userdefined)) {
    aIsBlock=((gHTMLElements[aId].IsMemberOf(kBlock))       ||
              (gHTMLElements[aId].IsMemberOf(kBlockEntity)) ||
              (gHTMLElements[aId].IsMemberOf(kHeading))     ||
              (gHTMLElements[aId].IsMemberOf(kPreformatted))||
              (gHTMLElements[aId].IsMemberOf(kList)));
  }
  else {
    aIsBlock = false;
  }

  return NS_OK;
}
