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
#include <ThreadManagerTests.h>

/*GetCurrentThread*/

TM_OJIAPITest(ThreadManager_GetCurrentThread_2) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->GetCurrentThread(NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no memory is allocated for the result pointer.");
	return TestResult::FAIL("GetCurrentThread", rc);

}

TM_OJIAPITest(ThreadManager_GetCurrentThread_1) {
	GET_TM_FOR_TEST
	PRUint32 *id;

	nsresult rc = threadMgr->GetCurrentThread((nsPluginThread**)&id);
	//printf("Current thread id %d\n", (*id));
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should work OK.");
	return TestResult::FAIL("GetCurrentThread", rc);

}
