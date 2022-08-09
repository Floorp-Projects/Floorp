/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <climits>
#include <cmath>
#include "FuzzingTraits.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"
#include "js/Exception.h"
#include "js/PropertyAndElement.h"  // JS_Enumerate, JS_GetProperty, JS_GetPropertyById, JS_SetProperty, JS_SetPropertyById
#include "prenv.h"
#include "MessageManagerFuzzer.h"
#include "mozilla/ErrorResult.h"
#include "nsComponentManagerUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsFrameMessageManager.h"
#include "nsJSUtils.h"
#include "nsXULAppAPI.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIFile.h"
#include "nsIFileStreams.h"
#include "nsILineInputStream.h"
#include "nsLocalFile.h"
#include "nsTArray.h"

#ifdef IsLoggingEnabled
// This is defined in the Windows SDK urlmon.h
#  undef IsLoggingEnabled
#endif

#define MESSAGEMANAGER_FUZZER_DEFAULT_MUTATION_PROBABILITY 2
#define MSGMGR_FUZZER_LOG(fmt, args...)                        \
  if (MessageManagerFuzzer::IsLoggingEnabled()) {              \
    printf_stderr("[MessageManagerFuzzer] " fmt "\n", ##args); \
  }

namespace mozilla {
namespace dom {

using namespace fuzzing;
using namespace ipc;

/* static */
void MessageManagerFuzzer::ReadFile(const char* path,
                                    nsTArray<nsCString>& aArray) {
  nsCOMPtr<nsIFile> file;
  nsresult rv =
      NS_NewLocalFile(NS_ConvertUTF8toUTF16(path), true, getter_AddRefs(file));
  NS_ENSURE_SUCCESS_VOID(rv);

  bool exists = false;
  rv = file->Exists(&exists);
  if (NS_FAILED(rv) || !exists) {
    return;
  }

  nsCOMPtr<nsIFileInputStream> fileStream(
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = fileStream->Init(file, -1, -1, false);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsILineInputStream> lineStream(do_QueryInterface(fileStream, &rv));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsAutoCString line;
  bool more = true;
  do {
    rv = lineStream->ReadLine(line, &more);
    NS_ENSURE_SUCCESS_VOID(rv);
    aArray.AppendElement(line);
  } while (more);
}

/* static */
bool MessageManagerFuzzer::IsMessageNameBlacklisted(
    const nsAString& aMessageName) {
  static bool sFileLoaded = false;
  static nsTArray<nsCString> valuesInFile;

  if (!sFileLoaded) {
    ReadFile(PR_GetEnv("MESSAGEMANAGER_FUZZER_BLACKLIST"), valuesInFile);
    sFileLoaded = true;
  }

  if (valuesInFile.Length() == 0) {
    return false;
  }

  return valuesInFile.Contains(NS_ConvertUTF16toUTF8(aMessageName).get());
}

/* static */
nsCString MessageManagerFuzzer::GetFuzzValueFromFile() {
  static bool sFileLoaded = false;
  static nsTArray<nsCString> valuesInFile;

  if (!sFileLoaded) {
    ReadFile(PR_GetEnv("MESSAGEMANAGER_FUZZER_STRINGSFILE"), valuesInFile);
    sFileLoaded = true;
  }

  // If something goes wrong with importing the file we return an empty string.
  if (valuesInFile.Length() == 0) {
    return nsCString();
  }

  unsigned randIdx = RandomIntegerRange<unsigned>(0, valuesInFile.Length());
  return valuesInFile.ElementAt(randIdx);
}

/* static */
void MessageManagerFuzzer::MutateObject(JSContext* aCx,
                                        JS::Handle<JS::Value> aValue,
                                        unsigned short int aRecursionCounter) {
  JS::Rooted<JSObject*> object(aCx, &aValue.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));

  if (!JS_Enumerate(aCx, object, &ids)) {
    return;
  }

  for (size_t i = 0, n = ids.length(); i < n; i++) {
    // Retrieve Property name.
    nsAutoJSString propName;
    if (!propName.init(aCx, ids[i])) {
      continue;
    }
    MSGMGR_FUZZER_LOG("%*s- Property: %s", aRecursionCounter * 4, "",
                      NS_ConvertUTF16toUTF8(propName).get());

    // The likelihood when a value gets fuzzed of this object.
    if (!FuzzingTraits::Sometimes(DefaultMutationProbability())) {
      continue;
    }

    // Retrieve Property value.
    JS::Rooted<JS::Value> propertyValue(aCx);
    JS_GetPropertyById(aCx, object, ids[i], &propertyValue);

    JS::Rooted<JS::Value> newPropValue(aCx);
    MutateValue(aCx, propertyValue, &newPropValue, aRecursionCounter);

    JS_SetPropertyById(aCx, object, ids[i], newPropValue);
  }
}

/* static */
bool MessageManagerFuzzer::MutateValue(
    JSContext* aCx, JS::Handle<JS::Value> aValue,
    JS::MutableHandle<JS::Value> aOutMutationValue,
    unsigned short int aRecursionCounter) {
  if (aValue.isInt32()) {
    if (FuzzingTraits::Sometimes(DefaultMutationProbability() * 2)) {
      aOutMutationValue.set(JS::Int32Value(RandomNumericLimit<int>()));
    } else {
      aOutMutationValue.set(JS::Int32Value(RandomInteger<int>()));
    }
    MSGMGR_FUZZER_LOG("%*s! Mutated value of type |int32|: '%d' to '%d'",
                      aRecursionCounter * 4, "", aValue.toInt32(),
                      aOutMutationValue.toInt32());
    return true;
  }

  if (aValue.isDouble()) {
    aOutMutationValue.set(JS::DoubleValue(RandomFloatingPoint<double>()));
    MSGMGR_FUZZER_LOG("%*s! Mutated value of type |double|: '%f' to '%f'",
                      aRecursionCounter * 4, "", aValue.toDouble(),
                      aOutMutationValue.toDouble());
    return true;
  }

  if (aValue.isBoolean()) {
    aOutMutationValue.set(JS::BooleanValue(bool(RandomIntegerRange(0, 2))));
    MSGMGR_FUZZER_LOG("%*s! Mutated value of type |boolean|: '%d' to '%d'",
                      aRecursionCounter * 4, "", aValue.toBoolean(),
                      aOutMutationValue.toBoolean());
    return true;
  }

  if (aValue.isString()) {
    nsCString x = GetFuzzValueFromFile();
    if (x.IsEmpty()) {
      return false;
    }
    JSString* str = JS_NewStringCopyZ(aCx, x.get());
    aOutMutationValue.set(JS::StringValue(str));
    JS::Rooted<JSString*> rootedValue(aCx, aValue.toString());
    JS::UniqueChars valueChars = JS_EncodeStringToUTF8(aCx, rootedValue);
    MSGMGR_FUZZER_LOG("%*s! Mutated value of type |string|: '%s' to '%s'",
                      aRecursionCounter * 4, "", valueChars.get(), x.get());
    return true;
  }

  if (aValue.isObject()) {
    aRecursionCounter++;
    MSGMGR_FUZZER_LOG("%*s<Enumerating found object>", aRecursionCounter * 4,
                      "");
    MutateObject(aCx, aValue, aRecursionCounter);
    aOutMutationValue.set(aValue);
    return true;
  }

  return false;
}

/* static */
bool MessageManagerFuzzer::Mutate(JSContext* aCx, const nsAString& aMessageName,
                                  ipc::StructuredCloneData* aData,
                                  const JS::Value& aTransfer) {
  MSGMGR_FUZZER_LOG("Message: %s in process: %d",
                    NS_ConvertUTF16toUTF8(aMessageName).get(),
                    XRE_GetProcessType());

  unsigned short int aRecursionCounter = 0;
  ErrorResult rv;
  JS::Rooted<JS::Value> t(aCx, aTransfer);

  /* Read original StructuredCloneData. */
  JS::Rooted<JS::Value> scdContent(aCx);
  aData->Read(aCx, &scdContent, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    JS_ClearPendingException(aCx);
    return false;
  }

  JS::Rooted<JS::Value> scdMutationContent(aCx);
  bool isMutated =
      MutateValue(aCx, scdContent, &scdMutationContent, aRecursionCounter);

  /* Write mutated StructuredCloneData. */
  ipc::StructuredCloneData mutatedStructuredCloneData;
  mutatedStructuredCloneData.Write(aCx, scdMutationContent, t,
                                   JS::CloneDataPolicy(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    JS_ClearPendingException(aCx);
    return false;
  }

  // See: https://bugzilla.mozilla.org/show_bug.cgi?id=1346040
  aData->Copy(mutatedStructuredCloneData);

  /* Mutated and successfully written to StructuredCloneData object. */
  if (isMutated) {
    JS::Rooted<JSString*> str(aCx, JS_ValueToSource(aCx, scdMutationContent));
    JS::UniqueChars strChars = JS_EncodeStringToUTF8(aCx, str);
    MSGMGR_FUZZER_LOG("Mutated '%s' Message: %s",
                      NS_ConvertUTF16toUTF8(aMessageName).get(),
                      strChars.get());
  }

  return true;
}

/* static */
unsigned int MessageManagerFuzzer::DefaultMutationProbability() {
  static unsigned long sPropValue =
      MESSAGEMANAGER_FUZZER_DEFAULT_MUTATION_PROBABILITY;
  static bool sInitialized = false;

  if (sInitialized) {
    return sPropValue;
  }
  sInitialized = true;

  // Defines the likelihood of fuzzing a message.
  const char* probability =
      PR_GetEnv("MESSAGEMANAGER_FUZZER_MUTATION_PROBABILITY");
  if (probability) {
    long n = std::strtol(probability, nullptr, 10);
    if (n != 0) {
      sPropValue = n;
      return sPropValue;
    }
  }

  return sPropValue;
}

/* static */
bool MessageManagerFuzzer::IsLoggingEnabled() {
  static bool sInitialized = false;
  static bool sIsLoggingEnabled = false;

  if (!sInitialized) {
    sIsLoggingEnabled = !!PR_GetEnv("MESSAGEMANAGER_FUZZER_ENABLE_LOGGING");
    sInitialized = true;
  }

  return sIsLoggingEnabled;
}

/* static */
bool MessageManagerFuzzer::IsEnabled() {
  return !!PR_GetEnv("MESSAGEMANAGER_FUZZER_ENABLE") && XRE_IsContentProcess();
}

/* static */
void MessageManagerFuzzer::TryMutate(JSContext* aCx,
                                     const nsAString& aMessageName,
                                     ipc::StructuredCloneData* aData,
                                     const JS::Value& aTransfer) {
  if (!IsEnabled()) {
    return;
  }

  if (IsMessageNameBlacklisted(aMessageName)) {
    MSGMGR_FUZZER_LOG("Blacklisted message: %s",
                      NS_ConvertUTF16toUTF8(aMessageName).get());
    return;
  }

  Mutate(aCx, aMessageName, aData, aTransfer);
}

}  // namespace dom
}  // namespace mozilla
