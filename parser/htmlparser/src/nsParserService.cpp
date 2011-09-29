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

extern "C" int MOZ_XMLCheckQName(const char* ptr, const char* end,
                                 int ns_aware, const char** colon);

nsParserService::nsParserService() : mEntries(0)
{
  mHaveNotifiedCategoryObservers = PR_FALSE;
}

nsParserService::~nsParserService()
{
  nsObserverEntry *entry = nsnull;
  while( (entry = static_cast<nsObserverEntry*>(mEntries.Pop())) ) {
    NS_RELEASE(entry);
  }
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
    aIsBlock = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::RegisterObserver(nsIElementObserver* aObserver,
                                  const nsAString& aTopic,
                                  const eHTMLTags* aTags)
{
  nsresult result = NS_OK;
  nsObserverEntry* entry = GetEntry(aTopic);

  if(!entry) {
    result = CreateEntry(aTopic,&entry);
    NS_ENSURE_SUCCESS(result,result);
  }

  while (*aTags) {
    if (*aTags <= NS_HTML_TAG_MAX) {
      entry->AddObserver(aObserver,*aTags);
    }
    ++aTags;
  }

  return result;
}

NS_IMETHODIMP
nsParserService::UnregisterObserver(nsIElementObserver* aObserver,
                                    const nsAString& aTopic)
{
  PRInt32 count = mEntries.GetSize();

  for (PRInt32 i=0; i < count; ++i) {
    nsObserverEntry* entry = static_cast<nsObserverEntry*>(mEntries.ObjectAt(i));
    if (entry && entry->Matches(aTopic)) {
      entry->RemoveObserver(aObserver);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsParserService::GetTopicObservers(const nsAString& aTopic,
                                   nsIObserverEntry** aEntry) {
  nsresult result = NS_OK;
  nsObserverEntry* entry = GetEntry(aTopic);

  if (!entry) {
    return NS_ERROR_NULL_POINTER;
  }

  NS_ADDREF(*aEntry = entry);

  return result;
}

nsresult
nsParserService::CheckQName(const nsAString& aQName,
                            bool aNamespaceAware,
                            const PRUnichar** aColon)
{
  const char* colon;
  const PRUnichar *begin, *end;
  begin = aQName.BeginReading();
  end = aQName.EndReading();
  int result = MOZ_XMLCheckQName(reinterpret_cast<const char*>(begin),
                                 reinterpret_cast<const char*>(end),
                                 aNamespaceAware, &colon);
  *aColon = reinterpret_cast<const PRUnichar*>(colon);

  if (result == 0) {
    return NS_OK;
  }

  // MOZ_EXPAT_EMPTY_QNAME || MOZ_EXPAT_INVALID_CHARACTER
  if (result == (1 << 0) || result == (1 << 1)) {
    return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
  }

  return NS_ERROR_DOM_NAMESPACE_ERR;
}

class nsMatchesTopic : public nsDequeFunctor{
  const nsAString& mString;
public:
  bool matched;
  nsObserverEntry* entry;
  nsMatchesTopic(const nsAString& aString):mString(aString),matched(PR_FALSE){}
  virtual void* operator()(void* anObject){
    entry=static_cast<nsObserverEntry*>(anObject);
    matched=mString.Equals(entry->mTopic);
    return matched ? nsnull : anObject;
  }
};

// XXX This may be more efficient as a HashTable instead of linear search
nsObserverEntry*
nsParserService::GetEntry(const nsAString& aTopic)
{
  if (!mHaveNotifiedCategoryObservers) {
    mHaveNotifiedCategoryObservers = PR_TRUE;
    NS_CreateServicesFromCategory("parser-service-category",
                                  static_cast<nsISupports*>(static_cast<void*>(this)),
                                  "parser-service-start"); 
  }

  nsMatchesTopic matchesTopic(aTopic);
  mEntries.FirstThat(*&matchesTopic);
  return matchesTopic.matched?matchesTopic.entry:nsnull;
}

nsresult
nsParserService::CreateEntry(const nsAString& aTopic, nsObserverEntry** aEntry)
{
  *aEntry = new nsObserverEntry(aTopic);

  if (!*aEntry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aEntry);
  mEntries.Push(*aEntry);

  return NS_OK;
}
