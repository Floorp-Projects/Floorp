/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prtypes.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "jArray.h"
#include "nsHtml5Portability.h"

nsIAtom*
nsHtml5Portability::newLocalNameFromBuffer(PRUnichar* buf, PRInt32 offset, PRInt32 length, nsHtml5AtomTable* interner)
{
  NS_ASSERTION(!offset, "The offset should always be zero here.");
  NS_ASSERTION(interner, "Didn't get an atom service.");
  return interner->GetAtom(nsDependentSubstring(buf, buf + length));
}

nsString*
nsHtml5Portability::newStringFromBuffer(PRUnichar* buf, PRInt32 offset, PRInt32 length)
{
  return new nsString(buf + offset, length);
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

jArray<PRUnichar,PRInt32>
nsHtml5Portability::newCharArrayFromLocal(nsIAtom* local)
{
  nsAutoString temp;
  local->ToString(temp);
  PRInt32 len = temp.Length();
  jArray<PRUnichar,PRInt32> arr = jArray<PRUnichar,PRInt32>::newJArray(len);
  memcpy(arr, temp.BeginReading(), len * sizeof(PRUnichar));
  return arr;
}

jArray<PRUnichar,PRInt32>
nsHtml5Portability::newCharArrayFromString(nsString* string)
{
  PRInt32 len = string->Length();
  jArray<PRUnichar,PRInt32> arr = jArray<PRUnichar,PRInt32>::newJArray(len);
  memcpy(arr, string->BeginReading(), len * sizeof(PRUnichar));
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
nsHtml5Portability::localEqualsBuffer(nsIAtom* local, PRUnichar* buf, PRInt32 offset, PRInt32 length)
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
  const PRUnichar* strPtr = string->BeginReading();
  const PRUnichar* end = string->EndReading();
  PRUnichar litChar;
  while ((litChar = *litPtr)) {
    NS_ASSERTION(!(litChar >= 'A' && litChar <= 'Z'), "Literal isn't in lower case.");
    if (strPtr == end) {
      return false;
    }
    PRUnichar strChar = *strPtr;
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
