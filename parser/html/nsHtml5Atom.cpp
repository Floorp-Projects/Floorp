/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5Atom.h"
#include "nsAutoPtr.h"

nsHtml5Atom::nsHtml5Atom(const nsAString& aString)
{
  mLength = aString.Length();
  nsRefPtr<nsStringBuffer> buf = nsStringBuffer::FromString(aString);
  if (buf) {
    mString = static_cast<PRUnichar*>(buf->Data());
  } else {
    buf = nsStringBuffer::Alloc((mLength + 1) * sizeof(PRUnichar));
    mString = static_cast<PRUnichar*>(buf->Data());
    CopyUnicodeTo(aString, 0, mString, mLength);
    mString[mLength] = PRUnichar(0);
  }

  NS_ASSERTION(mString[mLength] == PRUnichar(0), "null terminated");
  NS_ASSERTION(buf && buf->StorageSize() >= (mLength+1) * sizeof(PRUnichar),
               "enough storage");
  NS_ASSERTION(Equals(aString), "correct data");

  // Take ownership of buffer
  buf.forget();
}

nsHtml5Atom::~nsHtml5Atom()
{
  nsStringBuffer::FromData(mString)->Release();
}

NS_IMETHODIMP_(nsrefcnt)
nsHtml5Atom::AddRef()
{
  NS_NOTREACHED("Attempt to AddRef an nsHtml5Atom.");
  return 2;
}

NS_IMETHODIMP_(nsrefcnt)
nsHtml5Atom::Release()
{
  NS_NOTREACHED("Attempt to Release an nsHtml5Atom.");
  return 1;
}

NS_IMETHODIMP
nsHtml5Atom::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_NOTREACHED("Attempt to call QueryInterface an nsHtml5Atom.");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP 
nsHtml5Atom::ScriptableToString(nsAString& aBuf)
{
  NS_NOTREACHED("Should not call ScriptableToString.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Atom::ToUTF8String(nsACString& aReturn)
{
  NS_NOTREACHED("Should not attempt to convert to an UTF-8 string.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
nsHtml5Atom::IsStaticAtom()
{
  return false;
}

NS_IMETHODIMP
nsHtml5Atom::ScriptableEquals(const nsAString& aString, bool* aResult)
{
  NS_NOTREACHED("Should not call ScriptableEquals.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(bool)
nsHtml5Atom::EqualsUTF8(const nsACString& aString)
{
  NS_NOTREACHED("Should not attempt to compare with an UTF-8 string.");
  return false;
}
