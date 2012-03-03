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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Voll <jvoll@mozilla.com>
 *   Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.repositories.android;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;

public class AndroidBrowserHistoryDataExtender extends CachedSQLiteOpenHelper {

  public static final String LOG_TAG = "SyncHistoryVisits";

  // Database Specifications.
  protected static final String DB_NAME = "history_extension_database";
  protected static final int SCHEMA_VERSION = 1;

  // History Table.
  public static final String   TBL_HISTORY_EXT = "HistoryExtension";
  public static final String   COL_GUID = "guid";
  public static final String   COL_VISITS = "visits";
  public static final String[] TBL_COLUMNS = { COL_GUID, COL_VISITS };

  private final RepoUtils.QueryHelper queryHelper;

  public AndroidBrowserHistoryDataExtender(Context context) {
    super(context, DB_NAME, null, SCHEMA_VERSION);
    this.queryHelper = new RepoUtils.QueryHelper(context, null, LOG_TAG);
  }

  @Override
  public void onCreate(SQLiteDatabase db) {
    String createTableSql = "CREATE TABLE " + TBL_HISTORY_EXT + " ("
        + COL_GUID + " TEXT PRIMARY KEY, "
        + COL_VISITS + " TEXT)";
    db.execSQL(createTableSql);
  }

  @Override
  public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
    // For now we'll just drop and recreate the tables.
    db.execSQL("DROP TABLE IF EXISTS " + TBL_HISTORY_EXT);
    onCreate(db);
  }

  public void wipe() {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    onUpgrade(db, SCHEMA_VERSION, SCHEMA_VERSION);
  }

  /**
   * Store visit data.
   *
   * If a row with GUID `guid` does not exist, insert a new row.
   * If a row with GUID `guid` does exist, replace the visits column.
   *
   * @param guid The GUID to store to.
   * @param visits New visits data.
   */
  public void store(String guid, JSONArray visits) {
    SQLiteDatabase db = this.getCachedWritableDatabase();

    ContentValues cv = new ContentValues();
    cv.put(COL_GUID, guid);
    if (visits == null) {
      cv.put(COL_VISITS, "[]");
    } else {
      cv.put(COL_VISITS, visits.toJSONString());
    }

    String where = COL_GUID + " = ?";
    String[] args = new String[] { guid };
    int rowsUpdated = db.update(TBL_HISTORY_EXT, cv, where, args);
    if (rowsUpdated >= 1) {
      Logger.debug(LOG_TAG, "Replaced history extension record for row with GUID " + guid);
    } else {
      long rowId = db.insert(TBL_HISTORY_EXT, null, cv);
      Logger.debug(LOG_TAG, "Inserted history extension record into row: " + rowId);
    }
  }

  /**
   * Fetch a row.
   *
   * @param guid The GUID of the row to fetch.
   * @return A Cursor.
   * @throws NullCursorException
   */
  public Cursor fetch(String guid) throws NullCursorException {
    String where = COL_GUID + " = ?";
    String[] args = new String[] { guid };

    SQLiteDatabase db = this.getCachedReadableDatabase();
    Cursor cur = queryHelper.safeQuery(db, ".fetch", TBL_HISTORY_EXT,
        TBL_COLUMNS,
        where, args);
    return cur;
  }

  public JSONArray visitsForGUID(String guid) throws NullCursorException {
    Logger.debug(LOG_TAG, "Fetching visits for GUID " + guid);
    Cursor visits = fetch(guid);
    try {
      if (!visits.moveToFirst()) {
        // Cursor is empty.
        return new JSONArray();
      } else {
        return RepoUtils.getJSONArrayFromCursor(visits, COL_VISITS);
      }
    } finally {
      visits.close();
    }
  }

  /**
   * Delete a row.
   *
   * @param guid The GUID of the row to delete.
   * @return The number of rows deleted, either 0 (if a row with this GUID does not exist) or 1.
   */
  public int delete(String guid) {
    String where = COL_GUID + " = ?";
    String[] args = new String[] { guid };

    SQLiteDatabase db = this.getCachedWritableDatabase();
    return db.delete(TBL_HISTORY_EXT, where, args);
  }

  /**
   * Fetch all rows.
   *
   * @return A Cursor.
   * @throws NullCursorException
   */
  public Cursor fetchAll() throws NullCursorException {
    SQLiteDatabase db = this.getCachedReadableDatabase();
    Cursor cur = queryHelper.safeQuery(db, ".fetchAll", TBL_HISTORY_EXT,
        TBL_COLUMNS,
        null, null);
    return cur;
  }
}
