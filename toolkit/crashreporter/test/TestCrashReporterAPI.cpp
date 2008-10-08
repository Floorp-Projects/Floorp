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

#include "nsExceptionHandler.h"

// Defined in nsExceptionHandler.cpp, but not normally exposed
namespace CrashReporter {
  bool GetAnnotation(const nsACString& key, nsACString& data);
}

#define ok(message, test) do {                   \
                               if (!(test))      \
                                 return message; \
                             } while (0)
#define equals(message, a, b) ok(message, a == b)
#define ok_nsresult(message, rv) ok(message, NS_SUCCEEDED(rv))
#define fail_nsresult(message, rv) ok(message, NS_FAILED(rv))
#define run_test(test) do { char *message = test(); tests_run++; \
                            if (message) return message; } while (0)
int tests_run;



char *
test_init_exception_handler()
{
  nsCOMPtr<nsILocalFile> lf;
  // we don't plan on launching the crash reporter in this app anyway,
  // so it's ok to pass a bogus nsILocalFile
  ok_nsresult("NS_NewNativeLocalFile", NS_NewNativeLocalFile(EmptyCString(),
                                                             PR_TRUE,
                                                             getter_AddRefs(lf)));

  ok_nsresult("CrashReporter::SetExceptionHandler",
            CrashReporter::SetExceptionHandler(lf, nsnull));
  return 0;
}

char *
test_set_minidump_path()
{
  nsresult rv;
  nsCOMPtr<nsIProperties> directoryService = 
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv);

  ok_nsresult("do_GetService", rv);

  nsCOMPtr<nsILocalFile> currentDirectory;
  rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                             NS_GET_IID(nsILocalFile),
                             getter_AddRefs(currentDirectory));
  ok_nsresult("directoryService->Get", rv);

  nsAutoString currentDirectoryPath;
  rv = currentDirectory->GetPath(currentDirectoryPath);
  ok_nsresult("currentDirectory->GetPath", rv);
  
  ok_nsresult("CrashReporter::SetMinidumpPath",
            CrashReporter::SetMinidumpPath(currentDirectoryPath));

  return 0;
}

char *
test_annotate_crash_report_basic()
{
  ok_nsresult("CrashReporter::AnnotateCrashReport: basic 1",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test"),
                                        NS_LITERAL_CSTRING("some data")));


  nsCAutoString result;
  ok("CrashReporter::GetAnnotation", CrashReporter::GetAnnotation(NS_LITERAL_CSTRING("test"),
                                                                  result));
  nsCString msg = result + NS_LITERAL_CSTRING(" == ") +
    NS_LITERAL_CSTRING("some data");
  equals((char*)PromiseFlatCString(msg).get(), result,
         NS_LITERAL_CSTRING("some data"));

  // now replace it with something else
  ok_nsresult("CrashReporter::AnnotateCrashReport: basic 2",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test"),
                                        NS_LITERAL_CSTRING("some other data")));


  ok("CrashReporter::GetAnnotation", CrashReporter::GetAnnotation(NS_LITERAL_CSTRING("test"),
                                                                  result));
  msg = result + NS_LITERAL_CSTRING(" == ") +
    NS_LITERAL_CSTRING("some other data");
  equals((char*)PromiseFlatCString(msg).get(), result,
         NS_LITERAL_CSTRING("some other data"));
  return 0;
}

char *
test_appendnotes_crash_report()
{
  // Append two notes
  ok_nsresult("CrashReporter::AppendAppNotesToCrashReport: 1",
              CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("some data")));

  
  ok_nsresult("CrashReporter::AppendAppNotesToCrashReport: 2",
              CrashReporter::AppendAppNotesToCrashReport(NS_LITERAL_CSTRING("some other data")));

  // ensure that the result is correct
  nsCAutoString result;
  ok("CrashReporter::GetAnnotation",
     CrashReporter::GetAnnotation(NS_LITERAL_CSTRING("Notes"),
                                  result));

  nsCString msg = result + NS_LITERAL_CSTRING(" == ") +
    NS_LITERAL_CSTRING("some datasome other data");
  equals((char*)PromiseFlatCString(msg).get(), result,
         NS_LITERAL_CSTRING("some datasome other data"));
  return 0;
}

char *
test_annotate_crash_report_invalid_equals()
{
  fail_nsresult("CrashReporter::AnnotateCrashReport: invalid = in key",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test=something"),
                                        NS_LITERAL_CSTRING("some data")));
  return 0;
}

char *
test_annotate_crash_report_invalid_cr()
{
  fail_nsresult("CrashReporter::AnnotateCrashReport: invalid \n in key",
     CrashReporter::AnnotateCrashReport(NS_LITERAL_CSTRING("test\nsomething"),
                                        NS_LITERAL_CSTRING("some data")));
  return 0;
}

char *
test_unset_exception_handler()
{
  ok_nsresult("CrashReporter::UnsetExceptionHandler",
            CrashReporter::UnsetExceptionHandler());
  return 0;
}

static char* all_tests()
{
  run_test(test_init_exception_handler);
  run_test(test_set_minidump_path);
  run_test(test_annotate_crash_report_basic);
  run_test(test_annotate_crash_report_invalid_equals);
  run_test(test_annotate_crash_report_invalid_cr);
  run_test(test_appendnotes_crash_report);
  run_test(test_unset_exception_handler);
  return 0;
}

int
main (int argc, char **argv)
{
  NS_InitXPCOM2(nsnull, nsnull, nsnull);

  PR_SetEnv("MOZ_CRASHREPORTER=1");

  char* result = all_tests();
  if (result != 0) {
    printf("TEST-UNEXPECTED-FAIL | %s | %s\n", __FILE__, result);
  }
  else {
    printf("TEST-PASS | %s | all tests passed\n", __FILE__);
  }
  printf("Tests run: %d\n", tests_run);
 
  NS_ShutdownXPCOM(nsnull);

  return result != 0;
}
