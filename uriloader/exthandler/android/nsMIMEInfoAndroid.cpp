/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsMIMEInfoAndroid.h"
#include "nsArrayUtils.h"
#include "nsISupportsUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsMIMEInfoAndroid, nsIMIMEInfo, nsIHandlerInfo)

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchDefaultWithFile(nsIFile* aFile) {
  return LaunchWithFile(aFile);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LoadUriInternal(nsIURI* aURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool nsMIMEInfoAndroid::GetMimeInfoForMimeType(const nsACString& aMimeType,
                                               nsMIMEInfoAndroid** aMimeInfo) {
  RefPtr<nsMIMEInfoAndroid> info = new nsMIMEInfoAndroid(aMimeType);
  info.forget(aMimeInfo);
  return false;
}

bool nsMIMEInfoAndroid::GetMimeInfoForFileExt(const nsACString& aFileExt,
                                              nsMIMEInfoAndroid** aMimeInfo) {
  nsCString mimeType;
  if (jni::IsAvailable()) {
    auto javaString = java::GeckoAppShell::GetMimeTypeFromExtensions(aFileExt);
    if (javaString) {
      mimeType = javaString->ToCString();
    }
  }

  // "*/*" means that the bridge didn't know.
  if (mimeType.Equals(nsDependentCString("*/*"),
                      nsCaseInsensitiveCStringComparator)) {
    return false;
  }

  bool found = GetMimeInfoForMimeType(mimeType, aMimeInfo);
  (*aMimeInfo)->SetPrimaryExtension(aFileExt);
  return found;
}

/**
 * Returns MIME info for the aURL, which may contain the whole URL or only a
 * protocol
 */
nsresult nsMIMEInfoAndroid::GetMimeInfoForURL(const nsACString& aURL,
                                              bool* found,
                                              nsIHandlerInfo** info) {
  nsMIMEInfoAndroid* mimeinfo = new nsMIMEInfoAndroid(aURL);
  NS_ADDREF(*info = mimeinfo);
  *found = false;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetType(nsACString& aType) {
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDescription(nsAString& aDesc) {
  aDesc.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetDescription(const nsAString& aDesc) {
  mDescription.Assign(aDesc);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredApplicationHandler(nsIHandlerApp** aApp) {
  *aApp = mPrefApp;
  NS_IF_ADDREF(*aApp);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredApplicationHandler(nsIHandlerApp* aApp) {
  mPrefApp = aApp;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleApplicationHandlers(
    nsIMutableArray** aHandlerApps) {
  if (!mHandlerApps) mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID);

  if (!mHandlerApps) return NS_ERROR_OUT_OF_MEMORY;

  *aHandlerApps = mHandlerApps;
  NS_IF_ADDREF(*aHandlerApps);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetHasDefaultHandler(bool* aHasDefault) {
  uint32_t len;
  *aHasDefault = false;
  if (!mHandlerApps) return NS_OK;

  if (NS_FAILED(mHandlerApps->GetLength(&len))) return NS_OK;

  if (len == 0) return NS_OK;

  *aHasDefault = true;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetDefaultDescription(nsAString& aDesc) {
  aDesc.Assign(u""_ns);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithURI(
    nsIURI* aURI, mozilla::dom::BrowsingContext* aBrowsingContext) {
  return mPrefApp->LaunchWithURI(aURI, aBrowsingContext);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPreferredAction(nsHandlerInfoAction* aPrefAction) {
  *aPrefAction = mPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPreferredAction(nsHandlerInfoAction aPrefAction) {
  mPrefAction = aPrefAction;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetAlwaysAskBeforeHandling(bool* aAlwaysAsk) {
  *aAlwaysAsk = mAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetAlwaysAskBeforeHandling(bool aAlwaysAsk) {
  mAlwaysAsk = aAlwaysAsk;
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetFileExtensions(nsIUTF8StringEnumerator** aResult) {
  return NS_NewUTF8StringEnumerator(aResult, &mExtensions, this);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetFileExtensions(const nsACString& aExtensions) {
  mExtensions.Clear();
  nsACString::const_iterator start, end;
  aExtensions.BeginReading(start);
  aExtensions.EndReading(end);
  while (start != end) {
    nsACString::const_iterator cursor = start;
    mozilla::Unused << FindCharInReadable(',', cursor, end);
    AddUniqueExtension(Substring(start, cursor));
    // If a comma was found, skip it for the next search.
    start = cursor != end ? ++cursor : cursor;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::ExtensionExists(const nsACString& aExtension,
                                   bool* aRetVal) {
  NS_ASSERTION(!aExtension.IsEmpty(), "no extension");

  nsCString mimeType;
  if (jni::IsAvailable()) {
    auto javaString =
        java::GeckoAppShell::GetMimeTypeFromExtensions(aExtension);
    if (javaString) {
      mimeType = javaString->ToCString();
    }
  }

  // "*/*" means the bridge didn't find anything (i.e., extension doesn't
  // exist).
  *aRetVal = !mimeType.Equals(nsDependentCString("*/*"),
                              nsCaseInsensitiveCStringComparator);
  return NS_OK;
}

void nsMIMEInfoAndroid::AddUniqueExtension(const nsACString& aExtension) {
  if (!aExtension.IsEmpty() &&
      !mExtensions.Contains(aExtension,
                            nsCaseInsensitiveCStringArrayComparator())) {
    mExtensions.AppendElement(aExtension);
  }
}

NS_IMETHODIMP
nsMIMEInfoAndroid::AppendExtension(const nsACString& aExtension) {
  MOZ_ASSERT(!aExtension.IsEmpty(), "No extension");
  AddUniqueExtension(aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPrimaryExtension(nsACString& aPrimaryExtension) {
  if (!mExtensions.Length()) {
    aPrimaryExtension.Truncate();
    return NS_ERROR_NOT_INITIALIZED;
  }
  aPrimaryExtension = mExtensions[0];
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::SetPrimaryExtension(const nsACString& aExtension) {
  if (MOZ_UNLIKELY(aExtension.IsEmpty())) {
    // Don't assert since Java may return an empty extension for unknown types.
    return NS_ERROR_INVALID_ARG;
  }
  int32_t i = mExtensions.IndexOf(aExtension, 0,
                                  nsCaseInsensitiveCStringArrayComparator());
  if (i != -1) {
    mExtensions.RemoveElementAt(i);
  }
  mExtensions.InsertElementAt(0, aExtension);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetMIMEType(nsACString& aMIMEType) {
  aMIMEType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::Equals(nsIMIMEInfo* aMIMEInfo, bool* aRetVal) {
  if (!aMIMEInfo) return NS_ERROR_NULL_POINTER;

  nsAutoCString type;
  nsresult rv = aMIMEInfo->GetMIMEType(type);
  if (NS_FAILED(rv)) return rv;

  *aRetVal = mType.Equals(type);

  return NS_OK;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::GetPossibleLocalHandlers(nsIArray** aPossibleLocalHandlers) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMIMEInfoAndroid::LaunchWithFile(nsIFile* aFile) {
  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), aFile);
  return LoadUriInternal(uri);
}

NS_IMETHODIMP
nsMIMEInfoAndroid::IsCurrentAppOSDefault(bool* aRetVal) {
  // FIXME: this should in theory be meaningfully implemented. However, android
  // implements its own version of nsIHandlerApp instances which internally
  // have package and class names - but do not expose those. So to meaningfully
  // compare the handler app would require access to those and knowing what
  // our own package/class names are, and it's not clear how to do that.
  // It also seems less important to do this right on Android, given that
  // Android UI normally limits what apps you can associate with what files, so
  // it shouldn't be possible to get into the same kind of loop as on desktop.
  *aRetVal = false;
  return NS_OK;
}

nsMIMEInfoAndroid::nsMIMEInfoAndroid(const nsACString& aMIMEType)
    : mType(aMIMEType),
      mAlwaysAsk(true),
      mPrefAction(nsIMIMEInfo::useHelperApp) {
  mPrefApp = new nsMIMEInfoAndroid::SystemChooser(this);
  nsresult rv;
  mHandlerApps = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  mHandlerApps->AppendElement(mPrefApp);
}

#define SYSTEMCHOOSER_NAME u"Android chooser"
#define SYSTEMCHOOSER_DESCRIPTION \
  u"Android's default handler application chooser"

NS_IMPL_ISUPPORTS(nsMIMEInfoAndroid::SystemChooser, nsIHandlerApp)

nsresult nsMIMEInfoAndroid::SystemChooser::GetName(nsAString& aName) {
  aName.AssignLiteral(SYSTEMCHOOSER_NAME);
  return NS_OK;
}

nsresult nsMIMEInfoAndroid::SystemChooser::SetName(const nsAString&) {
  return NS_OK;
}

nsresult nsMIMEInfoAndroid::SystemChooser::GetDetailedDescription(
    nsAString& aDesc) {
  aDesc.AssignLiteral(SYSTEMCHOOSER_DESCRIPTION);
  return NS_OK;
}

nsresult nsMIMEInfoAndroid::SystemChooser::SetDetailedDescription(
    const nsAString&) {
  return NS_OK;
}

nsresult nsMIMEInfoAndroid::SystemChooser::Equals(nsIHandlerApp* aHandlerApp,
                                                  bool* aRetVal) {
  *aRetVal = false;
  if (!aHandlerApp) {
    return NS_OK;
  }

  nsAutoString name;
  nsAutoString detailedDescription;
  aHandlerApp->GetName(name);
  aHandlerApp->GetDetailedDescription(detailedDescription);

  *aRetVal = name.Equals(SYSTEMCHOOSER_NAME) &&
             detailedDescription.Equals(SYSTEMCHOOSER_DESCRIPTION);
  return NS_OK;
}

nsresult nsMIMEInfoAndroid::SystemChooser::LaunchWithURI(
    nsIURI* aURI, mozilla::dom::BrowsingContext*) {
  return mOuter->LoadUriInternal(aURI);
}
