/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdio.h>
#include <jni.h>
#include <android/log.h>
#include "dlfcn.h"
#include "APKOpen.h"
#ifndef MOZ_OLD_LINKER
#include "ElfLoader.h"
#endif
#include "SQLiteBridge.h"

#ifdef DEBUG
#define LOG(x...) __android_log_print(ANDROID_LOG_INFO, "GeckoJNI", x)
#else
#define LOG(x...)
#endif

#define SQLITE_WRAPPER_INT(name) name ## _t f_ ## name;

SQLITE_WRAPPER_INT(sqlite3_open)
SQLITE_WRAPPER_INT(sqlite3_errmsg)
SQLITE_WRAPPER_INT(sqlite3_prepare_v2)
SQLITE_WRAPPER_INT(sqlite3_bind_parameter_count)
SQLITE_WRAPPER_INT(sqlite3_bind_text)
SQLITE_WRAPPER_INT(sqlite3_step)
SQLITE_WRAPPER_INT(sqlite3_column_count)
SQLITE_WRAPPER_INT(sqlite3_finalize)
SQLITE_WRAPPER_INT(sqlite3_close)
SQLITE_WRAPPER_INT(sqlite3_column_name)
SQLITE_WRAPPER_INT(sqlite3_column_type)
SQLITE_WRAPPER_INT(sqlite3_column_blob)
SQLITE_WRAPPER_INT(sqlite3_column_bytes)
SQLITE_WRAPPER_INT(sqlite3_column_text)
SQLITE_WRAPPER_INT(sqlite3_changes)
SQLITE_WRAPPER_INT(sqlite3_last_insert_rowid)

void setup_sqlite_functions(void *sqlite_handle)
{
#define GETFUNC(name) f_ ## name = (name ## _t) __wrap_dlsym(sqlite_handle, #name)
  GETFUNC(sqlite3_open);
  GETFUNC(sqlite3_errmsg);
  GETFUNC(sqlite3_prepare_v2);
  GETFUNC(sqlite3_bind_parameter_count);
  GETFUNC(sqlite3_bind_text);
  GETFUNC(sqlite3_step);
  GETFUNC(sqlite3_column_count);
  GETFUNC(sqlite3_finalize);
  GETFUNC(sqlite3_close);
  GETFUNC(sqlite3_column_name);
  GETFUNC(sqlite3_column_type);
  GETFUNC(sqlite3_column_blob);
  GETFUNC(sqlite3_column_bytes);
  GETFUNC(sqlite3_column_text);
  GETFUNC(sqlite3_changes);
  GETFUNC(sqlite3_last_insert_rowid);
#undef GETFUNC
}

static bool initialized = false;
static jclass stringClass;
static jclass objectClass;
static jclass byteBufferClass;
static jclass cursorClass;
static jmethodID jByteBufferAllocateDirect;
static jmethodID jCursorConstructor;
static jmethodID jCursorAddRow;

static void
JNI_Setup(JNIEnv* jenv)
{
    if (initialized) return;

    jclass lObjectClass       = jenv->FindClass("java/lang/Object");
    jclass lStringClass       = jenv->FindClass("java/lang/String");
    jclass lByteBufferClass   = jenv->FindClass("java/nio/ByteBuffer");
    jclass lCursorClass       = jenv->FindClass("org/mozilla/gecko/sqlite/MatrixBlobCursor");

    if (lStringClass == NULL
        || lObjectClass == NULL
        || lByteBufferClass == NULL
        || lCursorClass == NULL) {
        LOG("Error finding classes");
        JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException",
                  "FindClass error");
        return;
    }

    // Those are only local references. Make them global so they work
    // across calls and threads.
    objectClass = (jclass)jenv->NewGlobalRef(lObjectClass);
    stringClass = (jclass)jenv->NewGlobalRef(lStringClass);
    byteBufferClass = (jclass)jenv->NewGlobalRef(lByteBufferClass);
    cursorClass = (jclass)jenv->NewGlobalRef(lCursorClass);

    if (stringClass == NULL || objectClass == NULL
        || byteBufferClass == NULL
        || cursorClass == NULL) {
        LOG("Error getting global references");
        JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException",
                  "NewGlobalRef error");
        return;
    }

    // public static ByteBuffer allocateDirect(int capacity)
    jByteBufferAllocateDirect =
        jenv->GetStaticMethodID(byteBufferClass, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
    // new MatrixBlobCursor(String [])
    jCursorConstructor =
        jenv->GetMethodID(cursorClass, "<init>", "([Ljava/lang/String;)V");
    // public void addRow (Object[] columnValues)
    jCursorAddRow =
        jenv->GetMethodID(cursorClass, "addRow", "([Ljava/lang/Object;)V");

    if (jByteBufferAllocateDirect == NULL
        || jCursorConstructor == NULL
        || jCursorAddRow == NULL) {
        LOG("Error finding methods");
        JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException",
                  "GetMethodId error");
        return;
    }

    initialized = true;
}

extern "C" NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_sqliteCall(JNIEnv* jenv, jclass,
                                                      jstring jDb,
                                                      jstring jQuery,
                                                      jobjectArray jParams,
                                                      jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    int rc;
    jobject jCursor = NULL;
    const char* dbPath;
    sqlite3 *db;
    char* errorMsg;

    dbPath = jenv->GetStringUTFChars(jDb, NULL);
    rc = f_sqlite3_open(dbPath, &db);
    jenv->ReleaseStringUTFChars(jDb, dbPath);
    if (rc != SQLITE_OK) {
        asprintf(&errorMsg, "Can't open database: %s\n", f_sqlite3_errmsg(db));
        LOG("Error in SQLiteBridge: %s\n", errorMsg);
        JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException", errorMsg);
        free(errorMsg);
    } else {
      jCursor = sqliteInternalCall(jenv, db, jQuery, jParams, jQueryRes);
    }
    f_sqlite3_close(db);
    return jCursor;
}

extern "C" NS_EXPORT jobject JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_sqliteCallWithDb(JNIEnv* jenv, jclass,
                                                            jlong jDb,
                                                            jstring jQuery,
                                                            jobjectArray jParams,
                                                            jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    jobject jCursor = NULL;
    sqlite3 *db = (sqlite3*)jDb;
    jCursor = sqliteInternalCall(jenv, db, jQuery, jParams, jQueryRes);
    return jCursor;
}

extern "C" NS_EXPORT jlong JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_openDatabase(JNIEnv* jenv, jclass,
                                                        jstring jDb)
{
    JNI_Setup(jenv);

    int rc;
    const char* dbPath;
    sqlite3 *db;
    char* errorMsg;

    dbPath = jenv->GetStringUTFChars(jDb, NULL);
    rc = f_sqlite3_open(dbPath, &db);
    jenv->ReleaseStringUTFChars(jDb, dbPath);
    if (rc != SQLITE_OK) {
        asprintf(&errorMsg, "Can't open database: %s\n", f_sqlite3_errmsg(db));
        LOG("Error in SQLiteBridge: %s\n", errorMsg);
        JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException", errorMsg);
        free(errorMsg);
    }
    return (jlong)db;
}

extern "C" NS_EXPORT void JNICALL
Java_org_mozilla_gecko_sqlite_SQLiteBridge_closeDatabase(JNIEnv* jenv, jclass,
                                                        jlong jDb)
{
    JNI_Setup(jenv);

    sqlite3 *db = (sqlite3*)jDb;
    f_sqlite3_close(db);
}

static jobject
sqliteInternalCall(JNIEnv* jenv,
                   sqlite3 *db,
                   jstring jQuery,
                   jobjectArray jParams,
                   jlongArray jQueryRes)
{
    JNI_Setup(jenv);

    jobject jCursor = NULL;
    char* errorMsg;
    jsize numPars = 0;

    const char *pzTail;
    sqlite3_stmt *ppStmt;
    int rc;

    const char* queryStr;
    queryStr = jenv->GetStringUTFChars(jQuery, NULL);

    rc = f_sqlite3_prepare_v2(db, queryStr, -1, &ppStmt, &pzTail);
    if (rc != SQLITE_OK || ppStmt == NULL) {
        asprintf(&errorMsg, "Can't prepare statement: %s\n", f_sqlite3_errmsg(db));
        goto error_close;
    }
    jenv->ReleaseStringUTFChars(jQuery, queryStr);

    // Check if number of parameters matches
    if (jParams != NULL) {
        numPars = jenv->GetArrayLength(jParams);
    }
    int sqlNumPars;
    sqlNumPars = f_sqlite3_bind_parameter_count(ppStmt);
    if (numPars != sqlNumPars) {
        asprintf(&errorMsg, "Passed parameter count (%d) doesn't match SQL parameter count (%d)\n",
            numPars, sqlNumPars);
        goto error_close;
    }

    if (jParams != NULL) {
        // Bind parameters, if any
        if (numPars > 0) {
            for (int i = 0; i < numPars; i++) {
                jobject jObjectParam = jenv->GetObjectArrayElement(jParams, i);
                // IsInstanceOf or isAssignableFrom? String is final, so IsInstanceOf
                // should be OK.
                jboolean isString = jenv->IsInstanceOf(jObjectParam, stringClass);
                if (isString != JNI_TRUE) {
                    asprintf(&errorMsg, "Parameter is not of String type");
                    goto error_close;
                }
                jstring jStringParam = (jstring)jObjectParam;
                const char* paramStr = jenv->GetStringUTFChars(jStringParam, NULL);
                // SQLite parameters index from 1.
                rc = f_sqlite3_bind_text(ppStmt, i + 1, paramStr, -1, SQLITE_TRANSIENT);
                jenv->ReleaseStringUTFChars(jStringParam, paramStr);
                if (rc != SQLITE_OK) {
                    asprintf(&errorMsg, "Error binding query parameter");
                    goto error_close;
                }
            }
        }
    }

    // Execute the query and step through the results
    rc = f_sqlite3_step(ppStmt);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        asprintf(&errorMsg, "Can't step statement: (%d) %s\n", rc, f_sqlite3_errmsg(db));
        goto error_close;
    }

    // Get the column count and names
    int cols;
    cols = f_sqlite3_column_count(ppStmt);

    {
        // Allocate a String[cols]
        jobjectArray jStringArray = jenv->NewObjectArray(cols,
                                                         stringClass,
                                                         NULL);
        if (jStringArray == NULL) {
            asprintf(&errorMsg, "Can't allocate String[]\n");
            goto error_close;
        }

        // Assign column names to the String[]
        for (int i = 0; i < cols; i++) {
            const char* colName = f_sqlite3_column_name(ppStmt, i);
            jstring jStr = jenv->NewStringUTF(colName);
            jenv->SetObjectArrayElement(jStringArray, i, jStr);
        }

        // Construct the MatrixCursor(String[]) with given column names
        jCursor = jenv->NewObject(cursorClass,
                                  jCursorConstructor,
                                  jStringArray);
        if (jCursor == NULL) {
            asprintf(&errorMsg, "Can't allocate MatrixBlobCursor\n");
            goto error_close;
        }
    }

    // Return the id and number of changed rows in jQueryRes
    {
        jlong id = f_sqlite3_last_insert_rowid(db);
        jenv->SetLongArrayRegion(jQueryRes, 0, 1, &id);

        jlong changed = f_sqlite3_changes(db);
        jenv->SetLongArrayRegion(jQueryRes, 1, 1, &changed);
    }

    // For each row, add an Object[] to the passed ArrayList,
    // with that containing either String or ByteArray objects
    // containing the columns
    while (rc != SQLITE_DONE) {
        // Process row
        // Construct Object[]
        jobjectArray jRow = jenv->NewObjectArray(cols,
                                                 objectClass,
                                                 NULL);
        if (jRow == NULL) {
            asprintf(&errorMsg, "Can't allocate jRow Object[]\n");
            goto error_close;
        }

        for (int i = 0; i < cols; i++) {
            int colType = f_sqlite3_column_type(ppStmt, i);
            if (colType == SQLITE_BLOB) {
                // Treat as blob
                const void* blob = f_sqlite3_column_blob(ppStmt, i);
                int colLen = f_sqlite3_column_bytes(ppStmt, i);

                // Construct ByteBuffer of correct size
                jobject jByteBuffer =
                    jenv->CallStaticObjectMethod(byteBufferClass,
                                                 jByteBufferAllocateDirect,
                                                 colLen);
                if (jByteBuffer == NULL) {
                    goto error_close;
                }

                // Get its backing array
                void* bufferArray = jenv->GetDirectBufferAddress(jByteBuffer);
                if (bufferArray == NULL) {
                    asprintf(&errorMsg, "Failure calling GetDirectBufferAddress\n");
                    goto error_close;
                }
                memcpy(bufferArray, blob, colLen);

                jenv->SetObjectArrayElement(jRow, i, jByteBuffer);
                jenv->DeleteLocalRef(jByteBuffer);
            } else if (colType == SQLITE_NULL) {
                jenv->SetObjectArrayElement(jRow, i, NULL);
            } else {
                // Treat everything else as text
                const char* txt = (const char*)f_sqlite3_column_text(ppStmt, i);
                jstring jStr = jenv->NewStringUTF(txt);
                jenv->SetObjectArrayElement(jRow, i, jStr);
                jenv->DeleteLocalRef(jStr);
            }
        }

        // Append Object[] to Cursor
        jenv->CallVoidMethod(jCursor, jCursorAddRow, jRow);

        // Clean up
        jenv->DeleteLocalRef(jRow);

        // Get next row
        rc = f_sqlite3_step(ppStmt);
        // Real error?
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            asprintf(&errorMsg, "Can't re-step statement:(%d) %s\n", rc, f_sqlite3_errmsg(db));
            goto error_close;
        }
    }

    rc = f_sqlite3_finalize(ppStmt);
    if (rc != SQLITE_OK) {
        asprintf(&errorMsg, "Can't finalize statement: %s\n", f_sqlite3_errmsg(db));
        goto error_close;
    }

    return jCursor;

error_close:
    LOG("Error in SQLiteBridge: %s\n", errorMsg);
    JNI_Throw(jenv, "org/mozilla/gecko/sqlite/SQLiteBridgeException", errorMsg);
    free(errorMsg);
    return jCursor;
}
