/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>

#include "nsXPCOM.h"
#include "nsINIParser.h"
#include "nsIFile.h"

static bool
StringCB(const char *aKey, const char *aValue, void* aClosure)
{
  printf("%s=%s\n", aKey, aValue);

  return true;
}

static bool
SectionCB(const char *aSection, void* aClosure)
{
  nsINIParser *ini = reinterpret_cast<nsINIParser*>(aClosure);

  printf("[%s]\n", aSection);

  ini->GetStrings(aSection, StringCB, nsnull);

  printf("\n");

  return true;
}

int main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <ini-file>\n", argv[0]);
    return 255;
  }

  nsCOMPtr<nsIFile> lf;

  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(argv[1]),
                                      true,
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

