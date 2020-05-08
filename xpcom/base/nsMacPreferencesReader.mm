/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMacPreferencesReader.h"

#include "js/JSON.h"
#include "JSONWriter.h"

NS_IMPL_ISUPPORTS(nsMacPreferencesReader, nsIMacPreferencesReader)

using namespace mozilla;

struct StringWriteFunc : public JSONWriteFunc {
  nsAString& mString;
  explicit StringWriteFunc(nsAString& aStr) : mString(aStr) {}
  void Write(const char* aStr) override { mString.Append(NS_ConvertUTF8toUTF16(aStr)); }
  void Write(const char* aStr, size_t aLen) override {
    mString.Append(NS_ConvertUTF8toUTF16(aStr, aLen));
  }
};

static void EvaluateDict(JSONWriter* aWriter, NSDictionary<NSString*, id>* aDict);

static void EvaluateArray(JSONWriter* aWriter, NSArray* aArray) {
  for (id elem in aArray) {
    if ([elem isKindOfClass:[NSString class]]) {
      aWriter->StringElement([elem UTF8String]);
    } else if ([elem isKindOfClass:[NSNumber class]]) {
      aWriter->IntElement([elem longLongValue]);
    } else if ([elem isKindOfClass:[NSArray class]]) {
      aWriter->StartArrayElement();
      EvaluateArray(aWriter, elem);
      aWriter->EndArray();
    } else if ([elem isKindOfClass:[NSDictionary class]]) {
      aWriter->StartObjectElement();
      EvaluateDict(aWriter, elem);
      aWriter->EndObject();
    }
  }
}

static void EvaluateDict(JSONWriter* aWriter, NSDictionary<NSString*, id>* aDict) {
  for (NSString* key in aDict) {
    id value = aDict[key];
    if ([value isKindOfClass:[NSString class]]) {
      aWriter->StringProperty([key UTF8String], [value UTF8String]);
    } else if ([value isKindOfClass:[NSNumber class]]) {
      aWriter->IntProperty([key UTF8String], [value longLongValue]);
    } else if ([value isKindOfClass:[NSArray class]]) {
      aWriter->StartArrayProperty([key UTF8String]);
      EvaluateArray(aWriter, value);
      aWriter->EndArray();
    } else if ([value isKindOfClass:[NSDictionary class]]) {
      aWriter->StartObjectProperty([key UTF8String]);
      EvaluateDict(aWriter, value);
      aWriter->EndObject();
    }
  }
}

NS_IMETHODIMP
nsMacPreferencesReader::PoliciesEnabled(bool* aPoliciesEnabled) {
  NSString* policiesEnabledStr = [NSString stringWithUTF8String:ENTERPRISE_POLICIES_ENABLED_KEY];
  *aPoliciesEnabled = [[NSUserDefaults standardUserDefaults] boolForKey:policiesEnabledStr] == YES;
  return NS_OK;
}

NS_IMETHODIMP
nsMacPreferencesReader::ReadPreferences(JSContext* aCx, JS::MutableHandle<JS::Value> aResult) {
  nsAutoString jsonStr;
  JSONWriter w(MakeUnique<StringWriteFunc>(jsonStr));
  w.Start();
  EvaluateDict(&w, [[NSUserDefaults standardUserDefaults] dictionaryRepresentation]);
  w.End();

  auto json = static_cast<const char16_t*>(jsonStr.get());

  JS::RootedValue val(aCx);
  MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, json, jsonStr.Length(), &val));

  aResult.set(val);
  return NS_OK;
}
