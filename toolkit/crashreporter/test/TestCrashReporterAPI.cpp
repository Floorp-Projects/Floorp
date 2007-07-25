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
 * The Original Code is Mozilla Breakpad integration
 *
 * The Initial Developer of the Original Code is
 * Ted Mielczarek <ted.mielczarek@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#include <stdlib.h>
#include <stdio.h>

#include "prenv.h"
#include "nsIComponentManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIProperties.h"
#include "nsCOMPtr.h"
#include "nsILocalFile.h"

#include "nsAirbagExceptionHandler.h"
#include "nsICrashReporter.h"

#define mu_assert(message, test) do { if (NS_FAILED(test)) \
                                       return message; } while (0)
#define mu_assert_failure(message, test) do { if (NS_SUCCEEDED(test)) \
                                               return message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)
int tests_run;

char *
test_init_exception_handler()
{
  mu_assert("CrashReporter::SetExceptionHandler",
            CrashReporter::SetExceptionHandler(nsnull));
  return 0;
}

char *
test_set_minidump_path()
{
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);

  mu_assert("do_GetService", rv);

  nsCOMPtr<nsILocalFile> currentDirectory;
  rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                             NS_GET_IID(nsILocalFile),
                             getter_AddRefs(currentDirectory));
  mu_assert("directoryService->Get", rv);

  nsAutoString currentDirectoryPath;
  rv = currentDirectory->GetPath(currentDirectoryPath);
  mu_assert("currentDirectory->GetPath", rv);
  
  mu_assert("CrashReporter::SetMinidumpPath",
            CrashReporter::SetMinidumpPath(currentDirectoryPath));

  return 0;
}

char *
test_annotate_crash_report_basic()
{
  mu_assert("CrashReporter::AnnotateCrashReport: basic",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test"),
                                        NS_LITERAL_CSTRING("some data")));

  return 0;
}

char *
test_annotate_crash_report_invalid_equals()
{
  mu_assert_failure("CrashReporter::AnnotateCrashReport: invalid = in key",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test=something"),
                                        NS_LITERAL_CSTRING("some data")));
  return 0;
}

char *
test_annotate_crash_report_invalid_cr()
{
  mu_assert_failure("CrashReporter::AnnotateCrashReport: invalid \n in key",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test\nsomething"),
                                        NS_LITERAL_CSTRING("some data")));
  return 0;
}

char *
test_unset_exception_handler()
{
  mu_assert("CrashReporter::UnsetExceptionHandler",
            CrashReporter::UnsetExceptionHandler());
  return 0;
}

static char* all_tests()
{
  mu_run_test(test_init_exception_handler);
  mu_run_test(test_set_minidump_path);
  mu_run_test(test_annotate_crash_report_basic);
  mu_run_test(test_annotate_crash_report_invalid_equals);
  mu_run_test(test_annotate_crash_report_invalid_cr);
  mu_run_test(test_unset_exception_handler);
  return 0;
}

int
main (int argc, char **argv)
{
  NS_InitXPCOM2(nsnull, nsnull, nsnull);

  char* env = new char[13];
  strcpy(env, "MOZ_AIRBAG=1");
  PR_SetEnv(env);

  char* result = all_tests();
  if (result != 0) {
    printf("FAIL: %s\n", result);
  }
  else {
    printf("ALL TESTS PASSED\n");
  }
  printf("Tests run: %d\n", tests_run);
 
  return result != 0;
}
