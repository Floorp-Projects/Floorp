/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
#ifndef ojiapitests_h___
#define ojiapitests_h___

//mozilla specific headers
#include "nspr.h"
#include "nsString.h"

#define OJI_TEST_RESULTS "OJITestResults.txt"
#define OJI_TESTS_LIST   "OJITests.lst"
#ifdef XP_UNIX
#define OJI_JNI_TESTS   "libojiapijnitests.so"
#define OJI_JM_TESTS    "libojiapijmtests.so"
#define OJI_TM_TESTS    "libojiapitmtests.so"
#define OJI_LCM_TESTS   "libojiapilcmtests.so"
#define OJI_JMPTI_TESTS "libojiapijmptitests.so"
#else
#define OJI_JNI_TESTS   "ojiapijnitests.dll"
#define OJI_JM_TESTS    "ojiapijmtests.dll"
#define OJI_TM_TESTS    "ojiapitmtests.dll"
#define OJI_LCM_TESTS   "ojiapilcmtests.dll"
#define OJI_JMPTI_TESTS "ojiapijmptitests.dll"
#endif


class TestResult;

typedef TestResult* (*TYPEOF_OJIAPITest)();

//may be used latter
typedef TestResult* (*TYPEOF_JNI_OJIAPITest)();
typedef TestResult* (*TYPEOF_JM_OJIAPITest)();
typedef TestResult* (*TYPEOF_LCM_OJIAPITest)();
typedef TestResult* (*TYPEOF_TM_OJIAPITest)();

//headers for tests
#define JNI_OJIAPITest(TestName)             \
extern "C" NS_EXPORT TestResult*             \
TestName()

#define TM_OJIAPITest(TestName)             \
extern "C" NS_EXPORT TestResult*             \
TestName()

#define JM_OJIAPITest(TestName)             \
extern "C" NS_EXPORT TestResult*             \
TestName()

#define LCM_OJIAPITest(TestName)             \
extern "C" NS_EXPORT TestResult*             \
TestName()

#define GET_JNI_FOR_TEST     \
JNIEnv* env;                 \
if (NS_FAILED(GetJNI(&env)) || !env) \
	return TestResult::FAIL("Can't get JNIEnv !");

#define GET_TM_FOR_TEST                      \
nsIThreadManager* threadMgr;                 \
if (NS_FAILED(GetThreadManager(&threadMgr)) || !threadMgr) \
	return TestResult::FAIL("Can't get TM !");


#define GET_JM_FOR_TEST                      \
nsIJVMManager* jvmMgr;                       \
if (NS_FAILED(GetJVMManager(&jvmMgr)) || !jvmMgr)       \
	return TestResult::FAIL("Can't get JNIEnv !");

#define GET_LCM_FOR_TEST                      \
nsILiveConnectManager* lcMgr;                 \
if (NS_FAILED(GetLiveConnectManager(&lcMgr)) || !lcMgr) \
	return TestResult::FAIL("Can't get JNIEnv !");


enum StatusValue {
  FAIL_value = 0,
  PASS_value = 1
};

class TestResult {
public:
  
/*
  static TestResult* PASS(nsString comment) {
    return new TestResult(PASS_value, comment); 
  }
*/

/** added by raju */
  static TestResult* PASS(char *comment) {
    char *msg = (char*)calloc(1, PL_strlen(comment)+1024);
    sprintf(msg, "Method %s", comment);
    CBufDescriptor *bufDescr = new CBufDescriptor((const char*)msg, PR_FALSE, PL_strlen(msg)+1, PL_strlen(msg));
    nsString *str = new nsAutoString(*bufDescr);
    return new TestResult(PASS_value, *str); 
  }

  static TestResult* FAIL(nsString comment) {
    return new TestResult(FAIL_value, comment); 
  }

  static TestResult* FAIL(char *method, nsresult rc) {
    char *comment = (char*)calloc(1, PL_strlen(method)+1024);
    sprintf(comment, "Method %s returned %X", method, rc);
    CBufDescriptor *bufDescr = new CBufDescriptor((const char*)comment, PR_FALSE, PL_strlen(comment)+1, PL_strlen(comment));
    nsString *str = new nsAutoString(*bufDescr);
    return new TestResult(FAIL_value, *str); 
  }

  static TestResult* FAIL(char *method, char* comment, nsresult rc) {
    char *outComment = (char*)calloc(1, PL_strlen(method)+1024);
    sprintf(outComment, "Method %s returned %X: %s", method, rc, comment);
    CBufDescriptor *bufDescr = new CBufDescriptor((const char*)comment, PR_FALSE, PL_strlen(comment)+1, PL_strlen(comment));
    nsString *str = new nsAutoString(*bufDescr);
    return new TestResult(FAIL_value, *str); 
  }

  static TestResult* FAIL(char *comment) {
      CBufDescriptor *bufDescr = new CBufDescriptor((const char*)comment, PR_FALSE, PL_strlen(comment)+1, PL_strlen(comment));
      nsString *str = new nsAutoString(*bufDescr);
      return new TestResult(FAIL_value, *str); 
  }

  TestResult ( StatusValue status, nsString comment) {
    this->status = status;
    this->comment = comment;
  }

  StatusValue status;
  nsString comment;

};

#endif //ojiapitests_h__
