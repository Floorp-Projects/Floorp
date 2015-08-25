/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIAtom.h"
#include "nsString.h"
#include "jArray.h"
#include "nsHtml5Portability.h"
#include "nsHtml5TreeBuilder.h"

nsIAtom*
nsHtml5Portability::newLocalNameFromBuffer(char16_t* buf, int32_t offset, int32_t length, nsHtml5AtomTable* interner)
{
  NS_ASSERTION(!offset, "The offset should always be zero here.");
  NS_ASSERTION(interner, "Didn't get an atom service.");
  return interner->GetAtom(nsDependentSubstring(buf, buf + length));
}

nsString*
nsHtml5Portability::newStringFromBuffer(char16_t* buf, int32_t offset, int32_t length, nsHtml5TreeBuilder* treeBuilder)
{
  nsString* str = new nsString();
  bool succeeded = str->Append(buf + offset, length, mozilla::fallible);
  if (!succeeded) {
    str->Assign(char16_t(0xFFFD));
    treeBuilder->MarkAsBroken(NS_ERROR_OUT_OF_MEMORY);
  }
  return str;
}

nsString*
nsHtml5Portability::newEmptyString()
{
  return new nsString();
}

nsString*
nsHtml5Portability::newStringFromLiteral(const char* literal)
{
  nsString* str = new nsString();
  str->AssignASCII(literal);
  return str;
}

nsString*
nsHtml5Portability::newStringFromString(nsString* string) {
  nsString* newStr = new nsString();
  newStr->Assign(*string);
  return newStr;
}

jArray<char16_t,int32_t>
nsHtml5Portability::newCharArrayFromLocal(nsIAtom* local)
{
  nsAutoString temp;
  local->ToString(temp);
  int32_t len = temp.Length();
  jArray<char16_t,int32_t> arr = jArray<char16_t,int32_t>::newJArray(len);
  memcpy(arr, temp.BeginReading(), len * sizeof(char16_t));
  return arr;
}

jArray<char16_t,int32_t>
nsHtml5Portability::newCharArrayFromString(nsString* string)
{
  int32_t len = string->Length();
  jArray<char16_t,int32_t> arr = jArray<char16_t,int32_t>::newJArray(len);
  memcpy(arr, string->BeginReading(), len * sizeof(char16_t));
  return arr;
}

nsIAtom*
nsHtml5Portability::newLocalFromLocal(nsIAtom* local, nsHtml5AtomTable* interner)
{
  NS_PRECONDITION(local, "Atom was null.");
  NS_PRECONDITION(interner, "Atom table was null");
  if (!local->IsStaticAtom()) {
    nsAutoString str;
    local->ToString(str);
    local = interner->GetAtom(str);
  }
  return local;
}

void
nsHtml5Portability::releaseString(nsString* str)
{
  delete str;
}

bool
nsHtml5Portability::localEqualsBuffer(nsIAtom* local, char16_t* buf, int32_t offset, int32_t length)
{
  return local->Equals(nsDependentSubstring(buf + offset, buf + offset + length));
}

bool
nsHtml5Portability::lowerCaseLiteralIsPrefixOfIgnoreAsciiCaseString(const char* lowerCaseLiteral, nsString* string)
{
  if (!string) {
    return false;
  }
  const char* litPtr = lowerCaseLiteral;
  const char16_t* strPtr = string->BeginReading();
  const char16_t* end = string->EndReading();
  char16_t litChar;
  while ((litChar = *litPtr)) {
    NS_ASSERTION(!(litChar >= 'A' && litChar <= 'Z'), "Literal isn't in lower case.");
    if (strPtr == end) {
      return false;
    }
    char16_t strChar = *strPtr;
    if (strChar >= 'A' && strChar <= 'Z') {
      strChar += 0x20;
    }
    if (litChar != strChar) {
      return false;
    }
    ++litPtr;
    ++strPtr;
  }
  return true;
}

bool
nsHtml5Portability::lowerCaseLiteralEqualsIgnoreAsciiCaseString(const char* lowerCaseLiteral, nsString* string)
{
  if (!string) {
    return false;
  }
  return string->LowerCaseEqualsASCII(lowerCaseLiteral);
}

bool
nsHtml5Portability::literalEqualsString(const char* literal, nsString* string)
{
  if (!string) {
    return false;
  }
  return string->EqualsASCII(literal);
}

bool
nsHtml5Portability::stringEqualsString(nsString* one, nsString* other)
{
  return one->Equals(*other);
}

void
nsHtml5Portability::initializeStatics()
{
}

void
nsHtml5Portability::releaseStatics()
{
}
