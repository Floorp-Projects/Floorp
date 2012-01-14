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
import org.mozilla.gecko.sync.repositories.NullCursorException;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

public class AndroidBrowserHistoryDataExtender extends SQLiteOpenHelper {

  public static final String TAG = "AndroidBrowserHistoryDataExtender";
  
  // Database Specifications.
  protected static final String DB_NAME = "history_extension_database";
  protected static final int SCHEMA_VERSION = 1;

  // History Table.
  public static final String TBL_HISTORY_EXT = "HistoryExtension";
  public static final String COL_GUID = "guid";
  public static final String COL_VISITS = "visits";
  
  protected AndroidBrowserHistoryDataExtender(Context context) {
    super(context, DB_NAME, null, SCHEMA_VERSION);
  }

  @Override
  public void onCreate(SQLiteDatabase db) {
    String createTableSql = "CREATE TABLE " + TBL_HISTORY_EXT + " ("
        + COL_GUID + " TEXT PRIMARY KEY, "
        + COL_VISITS + " TEXT)";
    db.execSQL(createTableSql);
  }

  // Cache these so we don't have to track them across cursors. Call `close`
  // when you're done.
  private static SQLiteDatabase readableDatabase;
  private static SQLiteDatabase writableDatabase;

  protected SQLiteDatabase getCachedReadableDatabase() {
    if (AndroidBrowserHistoryDataExtender.readableDatabase == null) {
      if (AndroidBrowserHistoryDataExtender.writableDatabase == null) {
        AndroidBrowserHistoryDataExtender.readableDatabase = this.getReadableDatabase();
        return AndroidBrowserHistoryDataExtender.readableDatabase;
      } else {
        return AndroidBrowserHistoryDataExtender.writableDatabase;
      }
    } else {
      return AndroidBrowserHistoryDataExtender.readableDatabase;
    }
  }

  protected SQLiteDatabase getCachedWritableDatabase() {
    if (AndroidBrowserHistoryDataExtender.writableDatabase == null) {
      AndroidBrowserHistoryDataExtender.writableDatabase = this.getWritableDatabase();
    }
    return AndroidBrowserHistoryDataExtender.writableDatabase;
  }

  @Override
  public void close() {
    if (AndroidBrowserHistoryDataExtender.readableDatabase != null) {
      AndroidBrowserHistoryDataExtender.readableDatabase.close();
      AndroidBrowserHistoryDataExtender.readableDatabase = null;
    }
    if (AndroidBrowserHistoryDataExtender.writableDatabase != null) {
      AndroidBrowserHistoryDataExtender.writableDatabase.close();
      AndroidBrowserHistoryDataExtender.writableDatabase = null;
    }
    super.close();
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

  // If a record with given GUID exists, we'll delete it
  // and store the updated version.
  public long store(String guid, JSONArray visits) {
    SQLiteDatabase db = this.getCachedReadableDatabase();
    
    // delete if exists
    delete(guid);
    
    // insert new
    ContentValues cv = new ContentValues();
    cv.put(COL_GUID, guid);
    if (visits == null) {
      cv.put(COL_VISITS, "[]");
    } else {
      cv.put(COL_VISITS, visits.toJSONString());
    }
    long rowId = db.insert(TBL_HISTORY_EXT, null, cv);
    Log.i(TAG, "Inserted history extension record into row: " + rowId);
    return rowId;
  }
  
  public Cursor fetch(String guid) throws NullCursorException {
    SQLiteDatabase db = this.getCachedReadableDatabase();
    long queryStart = System.currentTimeMillis();
    Cursor cur = db.query(TBL_HISTORY_EXT,
                          new String[] { COL_GUID, COL_VISITS },
                          COL_GUID + " = '" + guid + "'",
                          null, null, null, null);
    RepoUtils.queryTimeLogger("AndroidBrowserHistoryDataExtender.fetch(guid)", queryStart, System.currentTimeMillis());
    if (cur == null) {
      Log.e(TAG, "Got a null cursor while doing fetch for guid " + guid + " on history extension table");
      throw new NullCursorException(null);
    }
    return cur;
  }
  
  public void delete(String guid) {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    db.delete(TBL_HISTORY_EXT, COL_GUID + " = '" + guid + "'", null);
  }
}
