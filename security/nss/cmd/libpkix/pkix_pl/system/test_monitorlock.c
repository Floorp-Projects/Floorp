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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems
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
/*
 * test_monitorlock.c
 *
 * Tests basic MonitorLock object functionality. No multi-threading.
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

void createMonitorLockes(
        PKIX_PL_MonitorLock **monitorLock,
        PKIX_PL_MonitorLock **monitorLock2,
        PKIX_PL_MonitorLock **monitorLock3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Create
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Create
                (monitorLock2, plContext));

        *monitorLock3 = *monitorLock;
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Object_IncRef
                ((PKIX_PL_Object*)*monitorLock3, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

void testLock(PKIX_PL_MonitorLock *monitorLock)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Enter
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Enter
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Exit
                (monitorLock, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_MonitorLock_Exit
                (monitorLock, plContext));

cleanup:
        PKIX_TEST_RETURN();
}

void testDestroy(
        PKIX_PL_MonitorLock *monitorLock,
        PKIX_PL_MonitorLock *monitorLock2,
        PKIX_PL_MonitorLock *monitorLock3)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(monitorLock);
        PKIX_TEST_DECREF_BC(monitorLock2);
        PKIX_TEST_DECREF_BC(monitorLock3);

cleanup:
        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_PL_MonitorLock *monitorLock, *monitorLock2, *monitorLock3;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;
        PKIX_Boolean useArenas = PKIX_FALSE;

        PKIX_TEST_STD_VARS();

        startTests("MonitorLocks");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_PL_MonitorLock_Create");
        createMonitorLockes(&monitorLock, &monitorLock2, &monitorLock3);

        PKIX_TEST_EQ_HASH_TOSTR_DUP
                (monitorLock,
                monitorLock3,
                monitorLock2,
                NULL,
                MonitorLock,
                PKIX_FALSE);

        subTest("PKIX_PL_MonitorLock_Lock/Unlock");
        testLock(monitorLock);

        subTest("PKIX_PL_MonitorLock_Destroy");
        testDestroy(monitorLock, monitorLock2, monitorLock3);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("MonitorLockes");

        return (0);

}
