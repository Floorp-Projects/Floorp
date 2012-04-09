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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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

#ifndef SQLiteBridge_h
#define SQLiteBridge_h

#include "sqlite3.h"

void setup_sqlite_functions(void *sqlite_handle);
static jobject sqliteInternalCall(JNIEnv* jenv, sqlite3 *db, jstring jQuery, jobjectArray jParams, jlongArray jQueryRes);

#define SQLITE_WRAPPER(name, return_type, args...) \
typedef return_type (*name ## _t)(args);  \
extern name ## _t f_ ## name;

SQLITE_WRAPPER(sqlite3_open, int, const char*, sqlite3**)
SQLITE_WRAPPER(sqlite3_errmsg, const char*, sqlite3*)
SQLITE_WRAPPER(sqlite3_prepare_v2, int, sqlite3*, const char*, int, sqlite3_stmt**, const char**)
SQLITE_WRAPPER(sqlite3_bind_parameter_count, int, sqlite3_stmt*)
SQLITE_WRAPPER(sqlite3_bind_text, int, sqlite3_stmt*, int, const char*, int, void(*)(void*))
SQLITE_WRAPPER(sqlite3_step, int, sqlite3_stmt*)
SQLITE_WRAPPER(sqlite3_column_count, int, sqlite3_stmt*)
SQLITE_WRAPPER(sqlite3_finalize, int, sqlite3_stmt*)
SQLITE_WRAPPER(sqlite3_close, int, sqlite3*)
SQLITE_WRAPPER(sqlite3_column_name, const char*, sqlite3_stmt*, int)
SQLITE_WRAPPER(sqlite3_column_type, int, sqlite3_stmt*, int)
SQLITE_WRAPPER(sqlite3_column_blob, const void*, sqlite3_stmt*, int)
SQLITE_WRAPPER(sqlite3_column_bytes, int, sqlite3_stmt*, int)
SQLITE_WRAPPER(sqlite3_column_text, const unsigned char*, sqlite3_stmt*, int)
SQLITE_WRAPPER(sqlite3_changes, int, sqlite3*)
SQLITE_WRAPPER(sqlite3_last_insert_rowid, sqlite3_int64, sqlite3*)

#endif /* SQLiteBridge_h */
