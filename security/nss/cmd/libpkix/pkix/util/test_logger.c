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
 * test_logger.c
 *
 * Tests Logger Objects
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

void *plContext = NULL;

char *levels[] = {
        "None",
        "Fatal Error",
        "Error",
        "Warning",
        "Debug",
        "Trace"
};

PKIX_Error *testLoggerCallback(
        PKIX_Logger *logger,
        PKIX_PL_String *message,
        PKIX_UInt32 logLevel,
        PKIX_ERRORNUM logComponent,
        void *plContext)
{
        char *comp = NULL;
        char *msg = NULL;
        char result[100];
        static int callCount = 0;

        PKIX_TEST_STD_VARS();

        msg = PKIX_String2ASCII(message, plContext);
        PR_snprintf(result, 100, "Logging %s (%s): %s",
                levels[logLevel], PKIX_ERRORNAMES[logComponent], msg);
        subTest(result);

        callCount++;
        if (callCount > 1) {
                testError("Incorrect number of Logger Callback <expect 1>");
        }

cleanup:

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(msg, plContext));
        PKIX_TEST_RETURN();
}

PKIX_Error *testLoggerCallback2(
        PKIX_Logger *logger,
        PKIX_PL_String *message,
        PKIX_UInt32 logLevel,
        PKIX_ERRORNUM logComponent,
        void *plContext)
{
        char *comp = NULL;
        char *msg = NULL;
        char result[100];

        PKIX_TEST_STD_VARS();

        msg = PKIX_String2ASCII(message, plContext);
        PR_snprintf(result, 100, "Logging %s (%s): %s",
                levels[logLevel], PKIX_ERRORNAMES[logComponent], msg);
        subTest(result);

cleanup:
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(msg, plContext));
        PKIX_TEST_RETURN();
}

void
createLogger(PKIX_Logger **logger,
        PKIX_PL_Object *context,
        PKIX_Logger_LogCallback cb)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_Create
                (cb, context, logger, plContext));

cleanup:

        PKIX_TEST_RETURN();
}

void
testContextCallback(PKIX_Logger *logger, PKIX_Logger *logger2)
{
        PKIX_Logger_LogCallback cb = NULL;
        PKIX_PL_Object *context = NULL;
        PKIX_Boolean cmpResult = PKIX_FALSE;
        PKIX_UInt32 length;

        PKIX_TEST_STD_VARS();

        subTest("PKIX_Logger_GetLoggerContext");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetLoggerContext
                (logger2, &context, plContext));

        testEqualsHelper
                ((PKIX_PL_Object *)logger, context, PKIX_TRUE, plContext);

        subTest("PKIX_Logger_GetLogCallback");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetLogCallback
                (logger, &cb, plContext));

        if (cb != testLoggerCallback) {
                testError("Incorrect Logger Callback returned");
        }

cleanup:

        PKIX_TEST_DECREF_AC(context);
        PKIX_TEST_RETURN();
}

void
testComponent(PKIX_Logger *logger)
{
        PKIX_ERRORNUM compName = (PKIX_ERRORNUM)NULL;
        PKIX_ERRORNUM compNameReturn = (PKIX_ERRORNUM)NULL;
        PKIX_Boolean cmpResult = PKIX_FALSE;
        PKIX_TEST_STD_VARS();

        subTest("PKIX_Logger_GetLoggingComponent");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetLoggingComponent
                (logger, &compName, plContext));

        if (compName != (PKIX_ERRORNUM)NULL) {
                testError("Incorrect Logger Component returned. expect <NULL>");
        }

        subTest("PKIX_Logger_SetLoggingComponent");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetLoggingComponent
                (logger, PKIX_LIST_ERROR, plContext));

        subTest("PKIX_Logger_GetLoggingComponent");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetLoggingComponent
                (logger, &compNameReturn, plContext));

        if (compNameReturn != PKIX_LIST_ERROR) {
                testError("Incorrect Logger Component returned.");
        }

cleanup:

        PKIX_TEST_RETURN();
}

void
testMaxLoggingLevel(PKIX_Logger *logger)
{
        PKIX_UInt32 level = 0;
        PKIX_TEST_STD_VARS();

        subTest("PKIX_Logger_GetMaxLoggingLevel");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetMaxLoggingLevel
                (logger, &level, plContext));

        if (level != 0) {
                testError("Incorrect Logger MaxLoggingLevel returned");
        }

        subTest("PKIX_Logger_SetMaxLoggingLevel");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetMaxLoggingLevel
                (logger, 3, plContext));

        subTest("PKIX_Logger_GetMaxLoggingLevel");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetMaxLoggingLevel
                (logger, &level, plContext));

        if (level != 3) {
                testError("Incorrect Logger MaxLoggingLevel returned");
        }

cleanup:

        PKIX_TEST_RETURN();
}

void
testLogger(PKIX_Logger *logger, PKIX_Logger *logger2)
{
        PKIX_List *loggerList = NULL;
        PKIX_List *checkList = NULL;
        PKIX_UInt32 length;
        PKIX_Boolean cmpResult = PKIX_FALSE;
        char *expectedAscii = "[\n"
                "\tLogger: \n"
                "\tContext:          (null)\n"
                "\tMaximum Level:    3\n"
                "\tComponent Name:   LIST\n"
                "]\n";


        PKIX_TEST_STD_VARS();

        subTest("PKIX_GetLoggers");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_GetLoggers(&loggerList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (loggerList, &length, plContext));
        if (length != 0){
                testError("Incorrect Logger List returned");
        }
        PKIX_TEST_DECREF_BC(loggerList);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&loggerList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (loggerList, (PKIX_PL_Object *) logger, plContext));

        subTest("PKIX_SetLoggers");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_SetLoggers(loggerList, plContext));

        subTest("PKIX_Logger_SetLoggingComponent");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetLoggingComponent
                (logger2, PKIX_MUTEX_ERROR, plContext));

        subTest("PKIX_Logger_SetMaxLoggingLevel");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_SetMaxLoggingLevel
                (logger2, 5, plContext));

        subTest("PKIX_AddLogger");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_AddLogger(logger2, plContext));

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_Create(&checkList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (checkList, (PKIX_PL_Object *) logger, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_AppendItem
                (checkList, (PKIX_PL_Object *) logger2, plContext));

        PKIX_TEST_DECREF_BC(loggerList);

        subTest("PKIX_GetLoggers");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_GetLoggers(&loggerList, plContext));
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_List_GetLength
                (loggerList, &length, plContext));

        subTest("pkix_Loggers_Equals");
        testEqualsHelper
                ((PKIX_PL_Object *) loggerList,
                (PKIX_PL_Object *) checkList,
                PKIX_TRUE,
                plContext);

        subTest("pkix_Loggers_Duplicate");
        testDuplicateHelper((PKIX_PL_Object *)logger, plContext);

        subTest("pkix_Loggers_Hashcode");
        testHashcodeHelper((PKIX_PL_Object *) logger,
                           (PKIX_PL_Object *) logger,
                           PKIX_TRUE,
                           plContext);

        subTest("pkix_Loggers_ToString");
        testToStringHelper((PKIX_PL_Object *) logger, expectedAscii, plContext);

        subTest("PKIX Logger Callback");
        subTest("Expect to have ***Fatal Error (List): Null argument*** once");
        PKIX_TEST_EXPECT_ERROR(PKIX_List_AppendItem
                (NULL, (PKIX_PL_Object *) NULL, plContext));

cleanup:

        PKIX_TEST_DECREF_AC(loggerList);
        PKIX_TEST_DECREF_AC(checkList);
        PKIX_TEST_RETURN();
}
void
testDestroy(PKIX_Logger *logger)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(logger);

cleanup:

        PKIX_TEST_RETURN();
}

int main(int argc, char *argv[]) {

        PKIX_Logger *logger, *logger2;
        PKIX_UInt32 actualMinorVersion;
        PKIX_Boolean useArenas = PKIX_FALSE;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Loggers");

        useArenas = PKIX_TEST_ARENAS_ARG(argv[1]);

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Initialize
                                    (PKIX_TRUE, /* nssInitNeeded */
                                    useArenas,
                                    PKIX_MAJOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    PKIX_MINOR_VERSION,
                                    &actualMinorVersion,
                                    &plContext));

        subTest("PKIX_Logger_Create");
        createLogger(&logger, NULL, testLoggerCallback);
        createLogger(&logger2, (PKIX_PL_Object *)logger, testLoggerCallback2);

        subTest("Logger Context and Callback");
        testContextCallback(logger, logger2);

        subTest("Logger Component");
        testComponent(logger);

        subTest("Logger MaxLoggingLevel");
        testMaxLoggingLevel(logger);

        subTest("Logger List operations");
        testLogger(logger, logger2);

        subTest("PKIX_Logger_Destroy");
        testDestroy(logger);
        testDestroy(logger2);

cleanup:

        PKIX_Shutdown(plContext);

        PKIX_TEST_RETURN();

        endTests("Loggers");

        return (0);

}
