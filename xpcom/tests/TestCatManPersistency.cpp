/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Karsten Düsterloh <mnyromyr@tprac.de>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "TestHarness.h"
#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"

#define CMPT_CAT         "CatManPersistencyTestCategory"
#define CMPT_ENTRY       "CatManPersistencyTestEntry"
#define CMPT_VALUE_TRUE  "CatManPersistencyTestValueTrue"
#define CMPT_VALUE_FALSE "CatManPersistencyTestValueFalse"

int main(int argc, char** argv)
{
  // parameter 1 is the test phase to run:
  //   phase 1: initialize test category
  //   phase 2: check initialization, set new test value
  //   phase 3: check test value
  //   phase 4: delete test category
  //   phase 5: check deletion of test category

  if (argc < 2)
  {
    fail("no phase parameter");
    return NS_ERROR_FAILURE;
  }

  char phasename[] = "TestCatManPersistency Phase X";
  phasename[strlen(phasename) - 1] = argv[1][0];

  // initialize xpcom
  ScopedXPCOM xpcom(phasename);
  if (xpcom.failed())
  {
    fail("no ScopedXPCOM");
    return NS_ERROR_FAILURE;
  }

  // initialize catman
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
  {
    fail("no catman");
    return rv;
  }

  // execute phase
  nsCString previousValue;
  switch (argv[1][0])
  {
    case '1':
      //
      // Phase 1: Try to initialize our test category.
      //
      rv = catman->AddCategoryEntry(CMPT_CAT,
                                    CMPT_ENTRY,
                                    CMPT_VALUE_FALSE,
                                    PR_TRUE,  // persist
                                    PR_TRUE,  // replace
                                    nsnull);
      if (NS_FAILED(rv))
      {
        fail("AddCategoryEntry(false) failed");
        return rv;
      }
      break;

    case '2':
      // Phase 2: Try to set our test category value to true.
      //         Also check that it was not true before:
      //         If bug 364864 is fixed, it should have been false.
      //         If bug 364864 is not fixed, it should have been unset.
      rv = catman->AddCategoryEntry(CMPT_CAT,
                                    CMPT_ENTRY,
                                    CMPT_VALUE_TRUE,
                                    PR_TRUE,  // persist
                                    PR_TRUE,  // replace
                                    getter_Copies(previousValue));
      if (NS_FAILED(rv))
      {
        fail("AddCategoryEntry(true) failed");
        return rv;
      }
      if (strcmp(previousValue.get(), CMPT_VALUE_FALSE))
      {
        fail("initialization failed");
        return rv;
      }
      break;

    case '3':
      //
      // Phase 3: Check that our test category value is true.
      //
      rv = catman->GetCategoryEntry(CMPT_CAT,
                                    CMPT_ENTRY,
                                    getter_Copies(previousValue));
      if (NS_FAILED(rv))
      {
        fail("GetCategoryEntry failed");
        return rv;
      }
      if (strcmp(previousValue.get(), CMPT_VALUE_TRUE))
      {
        // bug 364864 isn't fixed?
        fail("setting value failed");
        return rv;
      }
      break;

    case '4':
      //
      // Phase 4: Try to delete our whole test category.
      //
      rv = catman->DeleteCategory(CMPT_CAT);
      if (NS_FAILED(rv))
      {
        fail("DeleteCategory failed");
        return rv;
      }
      break;

    case '5':
      //
      // Phase 5: Check that our test category value was deleted.
      //
      rv = catman->GetCategoryEntry(CMPT_CAT,
                                    CMPT_ENTRY,
                                    getter_Copies(previousValue));
      if (NS_SUCCEEDED(rv))
      {
        fail("category wasn't deleted");
        return rv;
      }
      break;

    default:
      fail("invalid phase parameter '%s'", argv[1]);
      return NS_ERROR_FAILURE;
  }


  //
  //  and everyone say: Yay!
  //
  passed(phasename);
  return NS_OK; // failure is a non-zero return
}
