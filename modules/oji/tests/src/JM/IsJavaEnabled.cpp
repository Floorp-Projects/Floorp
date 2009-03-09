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
#include "JVMManagerTests.h"

/*IsJavaEnabled*/

JM_OJIAPITest(JVMManager_IsJavaEnabled_1) {
	GET_JM_FOR_TEST

	nsresult rc = jvmMgr->IsJavaEnabled(nsnull);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no space is allocated for the result pointer.");
	return TestResult::FAIL("GetProxyJNI", rc);

}

JM_OJIAPITest(JVMManager_IsJavaEnabled_2) {
	GET_JM_FOR_TEST
	PRBool b;

	nsresult rc = jvmMgr->IsJavaEnabled(&b);
	if (NS_SUCCEEDED(rc) && b == PR_TRUE) {
		return TestResult::PASS("Should PASS if security.enable_java property is set to true or ommited.");
	}
	return TestResult::FAIL("IsJavaEnabled", rc);
}


JM_OJIAPITest(JVMManager_IsJavaEnabled_3) {
	GET_JM_FOR_TEST
	PRBool b;

	//how can we disable Java ????? -> this test doesn't work
	nsresult rc = jvmMgr->IsJavaEnabled(&b);
	if (NS_SUCCEEDED(rc) && b == PR_FALSE) {
		return TestResult::PASS("Should PASS if security.enable_java property is set to false.");
	}
	return TestResult::FAIL("To make this test pass one should delete OJI plugin !");

}







 
