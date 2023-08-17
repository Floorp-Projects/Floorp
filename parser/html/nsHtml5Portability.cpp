/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Portability.h"
#include "jArray.h"
#include "nsAtom.h"
#include "nsHtml5TreeBuilder.h"
#include "nsString.h"
#include "mozilla/CheckedInt.h"

int32_t nsHtml5Portability::checkedAdd(int32_t a, int32_t b) {
  mozilla::CheckedInt<int32_t> sum(a);
  sum += b;
  MOZ_RELEASE_ASSERT(sum.isValid(),
                     "HTML input too large for signed 32-bit integer.");
  return sum.value();
}

nsAtom* nsHtml5Portability::newLocalNameFromBuffer(char16_t* buf,
                                                   int32_t length,
                                                   nsHtml5AtomTable* interner) {
  NS_ASSERTION(interner, "Didn't get an atom service.");
  return interner->GetAtom(nsDependentSubstring(buf, buf + length));
}

nsHtml5String nsHtml5Portability::newStringFromBuffer(
    char16_t* buf, int32_t offset, int32_t length,
    nsHtml5TreeBuilder* treeBuilder, bool maybeAtomize) {
  if (!length) {
    return nsHtml5String::EmptyString();
  }
  if (maybeAtomize) {
    return nsHtml5String::FromAtom(
        NS_AtomizeMainThread(nsDependentSubstring(buf + offset, length)));
  }
  return nsHtml5String::FromBuffer(buf + offset, length, treeBuilder);
}

nsHtml5String nsHtml5Portability::newEmptyString() {
  return nsHtml5String::EmptyString();
}

nsHtml5String nsHtml5Portability::newStringFromLiteral(const char* literal) {
  return nsHtml5String::FromLiteral(literal);
}

nsHtml5String nsHtml5Portability::newStringFromString(nsHtml5String string) {
  return string.Clone();
}

jArray<char16_t, int32_t> nsHtml5Portability::newCharArrayFromLocal(
    nsAtom* local) {
  nsAutoString temp;
  local->ToString(temp);
  int32_t len = temp.Length();
  jArray<char16_t, int32_t> arr = jArray<char16_t, int32_t>::newJArray(len);
  memcpy(arr, temp.BeginReading(), len * sizeof(char16_t));
  return arr;
}

jArray<char16_t, int32_t> nsHtml5Portability::newCharArrayFromString(
    nsHtml5String string) {
  MOZ_RELEASE_ASSERT(string);
  uint32_t len = string.Length();
  MOZ_RELEASE_ASSERT(len < INT32_MAX);
  jArray<char16_t, int32_t> arr = jArray<char16_t, int32_t>::newJArray(len);
  string.CopyToBuffer(arr);
  return arr;
}

bool nsHtml5Portability::localEqualsBuffer(nsAtom* local, char16_t* buf,
                                           int32_t length) {
  return local->Equals(buf, length);
}

bool nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(
    const char* lowerCaseLiteral, nsHtml5String string) {
  return string.LowerCaseStartsWithASCII(lowerCaseLiteral);
}

bool nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(
    const char* lowerCaseLiteral, nsHtml5String string) {
  return string.LowerCaseEqualsASCII(lowerCaseLiteral);
}

bool nsHtml5Portability::literalEqualsString(const char* literal,
                                             nsHtml5String string) {
  return string.EqualsASCII(literal);
}

bool nsHtml5Portability::stringEqualsString(nsHtml5String one,
                                            nsHtml5String other) {
  return one.Equals(other);
}

void nsHtml5Portability::initializeStatics() {}

void nsHtml5Portability::releaseStatics() {}
