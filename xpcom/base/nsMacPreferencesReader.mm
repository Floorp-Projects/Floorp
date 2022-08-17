/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacStringHelpers.h"
#include "nsMacPreferencesReader.h"
#include "nsString.h"

#include "js/JSON.h"
#include "js/RootingAPI.h"
#include "js/Value.h"
#include "mozilla/JSONStringWriteFuncs.h"

NS_IMPL_ISUPPORTS(nsMacPreferencesReader, nsIMacPreferencesReader)

using namespace mozilla;

static void EvaluateDict(JSONWriter* aWriter, NSDictionary<NSString*, id>* aDict);

static void EvaluateArray(JSONWriter* aWriter, NSArray* aArray) {
  for (id elem in aArray) {
    if ([elem isKindOfClass:[NSString class]]) {
      aWriter->StringElement(MakeStringSpan([elem UTF8String]));
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
      aWriter->StringProperty(MakeStringSpan([key UTF8String]), MakeStringSpan([value UTF8String]));
    } else if ([value isKindOfClass:[NSNumber class]]) {
      aWriter->IntProperty(MakeStringSpan([key UTF8String]), [value longLongValue]);
    } else if ([value isKindOfClass:[NSArray class]]) {
      aWriter->StartArrayProperty(MakeStringSpan([key UTF8String]));
      EvaluateArray(aWriter, value);
      aWriter->EndArray();
    } else if ([value isKindOfClass:[NSDictionary class]]) {
      aWriter->StartObjectProperty(MakeStringSpan([key UTF8String]));
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
  JSONStringWriteFunc<nsAutoCString> jsonStr;
  JSONWriter w(jsonStr);
  w.Start();
  EvaluateDict(&w, [[NSUserDefaults standardUserDefaults] dictionaryRepresentation]);
  w.End();

  NS_ConvertUTF8toUTF16 jsonStr16(jsonStr.StringCRef());

  JS::RootedValue val(aCx);
  MOZ_ALWAYS_TRUE(JS_ParseJSON(aCx, jsonStr16.get(), jsonStr16.Length(), &val));

  aResult.set(val);
  return NS_OK;
}
