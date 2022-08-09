/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageManagerFuzzer_h__
#define mozilla_dom_MessageManagerFuzzer_h__

#include "jspubtd.h"
#include "nsAString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

namespace ipc {
class StructuredCloneData;
}

/*
Exposed environment variables:
MESSAGEMANAGER_FUZZER_ENABLE=1
MESSAGEMANAGER_FUZZER_ENABLE_LOGGING=1 (optional)
MESSAGEMANAGER_FUZZER_MUTATION_PROBABILITY=2 (optional)
MESSAGEMANAGER_FUZZER_STRINGSFILE=<path> (optional)
MESSAGEMANAGER_FUZZER_BLACKLIST=<path> (optional)
*/

#ifdef IsLoggingEnabled
// This is defined in the Windows SDK urlmon.h
#  undef IsLoggingEnabled
#endif

class MessageManagerFuzzer {
 public:
  static void TryMutate(JSContext* aCx, const nsAString& aMessageName,
                        ipc::StructuredCloneData* aData,
                        const JS::Value& aTransfer);

 private:
  static void ReadFile(const char* path, nsTArray<nsCString>& aArray);
  static nsCString GetFuzzValueFromFile();
  static bool IsMessageNameBlacklisted(const nsAString& aMessageName);
  static bool Mutate(JSContext* aCx, const nsAString& aMessageName,
                     ipc::StructuredCloneData* aData,
                     const JS::Value& aTransfer);
  static void Mutate(JSContext* aCx, JS::Rooted<JS::Value>& aMutation);
  static void MutateObject(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           unsigned short int aRecursionCounter);
  static bool MutateValue(JSContext* aCx, JS::Handle<JS::Value> aValue,
                          JS::MutableHandle<JS::Value> aOutMutationValue,
                          unsigned short int aRecursionCounter);
  static unsigned int DefaultMutationProbability();
  static nsAutoString ReadJSON(JSContext* aCx, const JS::Value& aJSON);
  static bool IsEnabled();
  static bool IsLoggingEnabled();
};

}  // namespace dom
}  // namespace mozilla

#endif
