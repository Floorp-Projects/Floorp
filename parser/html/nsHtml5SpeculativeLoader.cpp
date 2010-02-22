/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henri Sivonen <hsivonen@iki.fi>
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

#include "nsHtml5SpeculativeLoader.h"
#include "nsICSSLoader.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsICSSLoaderObserver.h"
#include "nsIDocument.h"

/**
 * Used if we need to pass an nsICSSLoaderObserver as parameter,
 * but don't really need its services
 */
class nsHtml5DummyCSSLoaderObserver : public nsICSSLoaderObserver {
public:
  NS_IMETHOD
  StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate, nsresult aStatus) {
      return NS_OK;
  }
  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS1(nsHtml5DummyCSSLoaderObserver, nsICSSLoaderObserver)

nsHtml5SpeculativeLoader::nsHtml5SpeculativeLoader(nsHtml5TreeOpExecutor* aExecutor)
  : mExecutor(aExecutor)
{
  MOZ_COUNT_CTOR(nsHtml5SpeculativeLoader);
  mPreloadedURLs.Init(23); // Mean # of preloadable resources per page on dmoz
}

nsHtml5SpeculativeLoader::~nsHtml5SpeculativeLoader()
{
  MOZ_COUNT_DTOR(nsHtml5SpeculativeLoader);
}

NS_IMPL_THREADSAFE_ADDREF(nsHtml5SpeculativeLoader)

NS_IMPL_THREADSAFE_RELEASE(nsHtml5SpeculativeLoader)

already_AddRefed<nsIURI>
nsHtml5SpeculativeLoader::ConvertIfNotPreloadedYet(const nsAString& aURL)
{
  nsIDocument* doc = mExecutor->GetDocument();
  if (!doc) {
    return nsnull;
  }
  nsIURI* base = doc->GetBaseURI();
  const nsCString& charset = doc->GetDocumentCharacterSet();
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, charset.get(), base);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create a URI");
    return nsnull;
  }
  nsCAutoString spec;
  uri->GetSpec(spec);
  if (mPreloadedURLs.Contains(spec)) {
    return nsnull;
  }
  mPreloadedURLs.Put(spec);
  nsIURI* retURI = uri;
  NS_ADDREF(retURI);
  return retURI;
}

void
nsHtml5SpeculativeLoader::PreloadScript(const nsAString& aURL,
                                        const nsAString& aCharset,
                                        const nsAString& aType)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  nsIDocument* doc = mExecutor->GetDocument();
  if (doc) {
    doc->ScriptLoader()->PreloadURI(uri, aCharset, aType);
  }
}

void
nsHtml5SpeculativeLoader::PreloadStyle(const nsAString& aURL,
                                       const nsAString& aCharset)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  nsCOMPtr<nsICSSLoaderObserver> obs = new nsHtml5DummyCSSLoaderObserver();
  nsIDocument* doc = mExecutor->GetDocument();
  if (doc) {
    doc->CSSLoader()->LoadSheet(uri, 
                                doc->NodePrincipal(),
                                NS_LossyConvertUTF16toASCII(aCharset),
                                obs);
  }
}

void
nsHtml5SpeculativeLoader::PreloadImage(const nsAString& aURL)
{
  nsCOMPtr<nsIURI> uri = ConvertIfNotPreloadedYet(aURL);
  if (!uri) {
    return;
  }
  nsIDocument* doc = mExecutor->GetDocument();
  if (doc) {
    doc->MaybePreLoadImage(uri);
  }
}

void
nsHtml5SpeculativeLoader::ProcessManifest(const nsAString& aURL)
{
  mExecutor->ProcessOfflineManifest(aURL);
}
