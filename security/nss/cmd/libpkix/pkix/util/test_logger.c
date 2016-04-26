/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * test_logger.c
 *
 * Tests Logger Objects
 *
 */

#include "testutil.h"
#include "testutil_nss.h"

static void *plContext = NULL;

static char *levels[] = {
        "None",
        "Fatal Error",
        "Error",
        "Warning",
        "Debug",
        "Trace"
};

static
PKIX_Error *testLoggerCallback(
        PKIX_Logger *logger,
        PKIX_PL_String *message,
        PKIX_UInt32 logLevel,
        PKIX_ERRORCLASS logComponent,
        void *plContext)
{
        char *comp = NULL;
        char *msg = NULL;
        char result[100];
        static int callCount = 0;

        PKIX_TEST_STD_VARS();

        msg = PKIX_String2ASCII(message, plContext);
        PR_snprintf(result, 100, "Logging %s (%s): %s",
                levels[logLevel], PKIX_ERRORCLASSNAMES[logComponent], msg);
        subTest(result);

        callCount++;
        if (callCount > 1) {
                testError("Incorrect number of Logger Callback <expect 1>");
        }

cleanup:

        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(msg, plContext));
        PKIX_TEST_RETURN();
}

static
PKIX_Error *testLoggerCallback2(
        PKIX_Logger *logger,
        PKIX_PL_String *message,
        PKIX_UInt32 logLevel,
        PKIX_ERRORCLASS logComponent,
        void *plContext)
{
        char *comp = NULL;
        char *msg = NULL;
        char result[100];

        PKIX_TEST_STD_VARS();

        msg = PKIX_String2ASCII(message, plContext);
        PR_snprintf(result, 100, "Logging %s (%s): %s",
                levels[logLevel], PKIX_ERRORCLASSNAMES[logComponent], msg);
        subTest(result);

cleanup:
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_PL_Free(msg, plContext));
        PKIX_TEST_RETURN();
}

static void
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

static void
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

static void
testComponent(PKIX_Logger *logger)
{
        PKIX_ERRORCLASS compName = (PKIX_ERRORCLASS)NULL;
        PKIX_ERRORCLASS compNameReturn = (PKIX_ERRORCLASS)NULL;
        PKIX_Boolean cmpResult = PKIX_FALSE;
        PKIX_TEST_STD_VARS();

        subTest("PKIX_Logger_GetLoggingComponent");
        PKIX_TEST_EXPECT_NO_ERROR(PKIX_Logger_GetLoggingComponent
                (logger, &compName, plContext));

        if (compName != (PKIX_ERRORCLASS)NULL) {
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

static void
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

static void
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

static void
testDestroy(PKIX_Logger *logger)
{
        PKIX_TEST_STD_VARS();

        PKIX_TEST_DECREF_BC(logger);

cleanup:

        PKIX_TEST_RETURN();
}

int test_logger(int argc, char *argv[]) {

        PKIX_Logger *logger, *logger2;
        PKIX_UInt32 actualMinorVersion;
        PKIX_UInt32 j = 0;

        PKIX_TEST_STD_VARS();

        startTests("Loggers");

        PKIX_TEST_EXPECT_NO_ERROR(
            PKIX_PL_NssContext_Create(0, PKIX_FALSE, NULL, &plContext));

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
