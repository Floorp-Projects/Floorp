/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHtml5Atom.h"

nsHtml5Atom::nsHtml5Atom(const nsAString& aString)
  : mData(aString)
{
}

nsHtml5Atom::~nsHtml5Atom()
{
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
nsHtml5Atom::ToString(nsAString& aReturn)
{
  aReturn.Assign(mData);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Atom::ToUTF8String(nsACString& aReturn)
{
  NS_NOTREACHED("Should not attempt to convert to an UTF-8 string.");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHtml5Atom::GetUTF8String(const char **aReturn)
{
  NS_NOTREACHED("Should not attempt to get a UTF-8 string from nsHtml5Atom");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(PRBool)
nsHtml5Atom::IsStaticAtom()
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsHtml5Atom::Equals(const nsAString& aString, PRBool *aReturn)
{
  *aReturn = mData.Equals(aString);
  return NS_OK;
}

NS_IMETHODIMP
nsHtml5Atom::EqualsUTF8(const nsACString& aString, PRBool *aReturn)
{
  NS_NOTREACHED("Should not attempt to compare with an UTF-8 string.");
  return NS_ERROR_NOT_IMPLEMENTED;
}
