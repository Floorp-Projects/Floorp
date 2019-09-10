/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEnvironment.h"
#include "prenv.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsPromiseFlatString.h"
#include "nsDependentString.h"
#include "nsNativeCharsetUtils.h"
#include "mozilla/Printf.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsEnvironment, nsIEnvironment)

nsresult nsEnvironment::Create(nsISupports* aOuter, REFNSIID aIID,
                               void** aResult) {
  nsresult rv;
  *aResult = nullptr;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  nsEnvironment* obj = new nsEnvironment();

  rv = obj->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    delete obj;
  }
  return rv;
}

nsEnvironment::~nsEnvironment() {}

NS_IMETHODIMP
nsEnvironment::Exists(const nsAString& aName, bool* aOutValue) {
  nsAutoCString nativeName;
  nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString nativeVal;
#if defined(XP_UNIX)
  /* For Unix/Linux platforms we follow the Unix definition:
   * An environment variable exists when |getenv()| returns a non-nullptr
   * value. An environment variable does not exist when |getenv()| returns
   * nullptr.
   */
  const char* value = PR_GetEnv(nativeName.get());
  *aOutValue = value && *value;
#else
  /* For non-Unix/Linux platforms we have to fall back to a
   * "portable" definition (which is incorrect for Unix/Linux!!!!)
   * which simply checks whether the string returned by |Get()| is empty
   * or not.
   */
  nsAutoString value;
  Get(aName, value);
  *aOutValue = !value.IsEmpty();
#endif /* XP_UNIX */

  return NS_OK;
}

NS_IMETHODIMP
nsEnvironment::Get(const nsAString& aName, nsAString& aOutValue) {
  nsAutoCString nativeName;
  nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString nativeVal;
  const char* value = PR_GetEnv(nativeName.get());
  if (value && *value) {
    rv = NS_CopyNativeToUnicode(nsDependentCString(value), aOutValue);
  } else {
    aOutValue.Truncate();
    rv = NS_OK;
  }

  return rv;
}

/* Environment strings must have static duration; We're gonna leak all of this
 * at shutdown: this is by design, caused how Unix/Linux implement environment
 * vars.
 */

typedef nsBaseHashtableET<nsCharPtrHashKey, char*> EnvEntryType;
typedef nsTHashtable<EnvEntryType> EnvHashType;

static EnvHashType* gEnvHash = nullptr;

static bool EnsureEnvHash() {
  if (gEnvHash) {
    return true;
  }

  gEnvHash = new EnvHashType;
  if (!gEnvHash) {
    return false;
  }

  return true;
}

NS_IMETHODIMP
nsEnvironment::Set(const nsAString& aName, const nsAString& aValue) {
  nsAutoCString nativeName;
  nsAutoCString nativeVal;

  nsresult rv = NS_CopyUnicodeToNative(aName, nativeName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = NS_CopyUnicodeToNative(aValue, nativeVal);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MutexAutoLock lock(mLock);

  if (!EnsureEnvHash()) {
    return NS_ERROR_UNEXPECTED;
  }

  EnvEntryType* entry = gEnvHash->PutEntry(nativeName.get());
  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SmprintfPointer newData =
      mozilla::Smprintf("%s=%s", nativeName.get(), nativeVal.get());
  if (!newData) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PR_SetEnv(newData.get());
  if (entry->mData) {
    free(entry->mData);
  }
  entry->mData = newData.release();
  return NS_OK;
}
