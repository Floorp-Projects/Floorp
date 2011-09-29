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
 * The Original Code is Mozilla XPCOM tests.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include <string.h>

#include "nsXPCOM.h"
#include "nsINIParser.h"
#include "nsILocalFile.h"

static bool
StringCB(const char *aKey, const char *aValue, void* aClosure)
{
  printf("%s=%s\n", aKey, aValue);

  return PR_TRUE;
}

static bool
SectionCB(const char *aSection, void* aClosure)
{
  nsINIParser *ini = reinterpret_cast<nsINIParser*>(aClosure);

  printf("[%s]\n", aSection);

  ini->GetStrings(aSection, StringCB, nsnull);

  printf("\n");

  return PR_TRUE;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <ini-file>\n", argv[0]);
    return 255;
  }

  nsCOMPtr<nsILocalFile> lf;

  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(argv[1]),
                                      PR_TRUE,
                                      getter_AddRefs(lf));
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Error: NS_NewNativeLocalFile failed\n");
    return 1;
  }

  nsINIParser ini;
  rv = ini.Init(lf);
  if (NS_FAILED(rv)) {
    fprintf(stderr, "Error: Init failed.");
    return 2;
  }

  ini.GetSections(SectionCB, &ini);

  return 0;
}

