/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrashAnnotations.h"

#include <algorithm>
#include <cstring>
#include <iterator>

using std::begin;
using std::end;
using std::find_if;

namespace CrashReporter {

bool AnnotationFromString(Annotation& aResult, const char* aValue) {
  auto elem = find_if(
      begin(kAnnotationStrings), end(kAnnotationStrings),
      [&aValue](const char* aString) { return strcmp(aString, aValue) == 0; });

  if (elem == end(kAnnotationStrings)) {
    return false;
  }

  aResult = static_cast<Annotation>(elem - begin(kAnnotationStrings));
  return true;
}

bool IsAnnotationWhitelistedForPing(Annotation aAnnotation) {
  auto elem = find_if(
      begin(kCrashPingWhitelist), end(kCrashPingWhitelist),
      [&aAnnotation](Annotation aElement) { return aElement == aAnnotation; });

  return elem != end(kCrashPingWhitelist);
}

}  // namespace CrashReporter
