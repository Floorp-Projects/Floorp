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
 * The Original Code is nsAboutProtocolUtils.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#include "nsIURI.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIAboutModule.h"

inline nsresult
NS_GetAboutModuleName(nsIURI *aAboutURI, nsCString& aModule)
{
#ifdef DEBUG
    {
        PRBool isAbout;
        NS_ASSERTION(NS_SUCCEEDED(aAboutURI->SchemeIs("about", &isAbout)) &&
                     isAbout,
                     "should be used only on about: URIs");
    }
#endif

    nsresult rv = aAboutURI->GetPath(aModule);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 f = aModule.FindCharInSet(NS_LITERAL_CSTRING("#?"));
    if (f != kNotFound) {
        aModule.Truncate(f);
    }

    // convert to lowercase, as all about: modules are lowercase
    ToLowerCase(aModule);
    return NS_OK;
}

inline nsresult
NS_GetAboutModule(nsIURI *aAboutURI, nsIAboutModule** aModule)
{
  NS_PRECONDITION(aAboutURI, "Must have URI");

  nsCAutoString contractID;
  nsresult rv = NS_GetAboutModuleName(aAboutURI, contractID);
  if (NS_FAILED(rv)) return rv;

  // look up a handler to deal with "what"
  contractID.Insert(NS_LITERAL_CSTRING(NS_ABOUT_MODULE_CONTRACTID_PREFIX), 0);

  return CallGetService(contractID.get(), aModule);
}
