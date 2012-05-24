/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;

public class ClientsDatabase extends CachedSQLiteOpenHelper {

  public static final String LOG_TAG = "ClientsDatabase";

  // Database Specifications.
  protected static final String DB_NAME = "clients_database";
  protected static final int SCHEMA_VERSION = 1;

  // Clients Table.
  public static final String TBL_CLIENTS      = "clients";
  public static final String COL_ACCOUNT_GUID = "guid";
  public static final String COL_PROFILE      = "profile";
  public static final String COL_NAME         = "name";
  public static final String COL_TYPE         = "device_type";

  public static final String[] TBL_COLUMNS = new String[] { COL_ACCOUNT_GUID, COL_PROFILE, COL_NAME, COL_TYPE };
  public static final String TBL_KEY = COL_ACCOUNT_GUID + " = ? AND " +
                                       COL_PROFILE + " = ?";

  private final RepoUtils.QueryHelper queryHelper;

  public ClientsDatabase(Context context) {
    super(context, DB_NAME, null, SCHEMA_VERSION);
    this.queryHelper = new RepoUtils.QueryHelper(context, null, LOG_TAG);
  }

  @Override
  public void onCreate(SQLiteDatabase db) {
    String createTableSql = "CREATE TABLE " + TBL_CLIENTS + " ("
        + COL_ACCOUNT_GUID + " TEXT, "
        + COL_PROFILE + " TEXT, "
        + COL_NAME + " TEXT, "
        + COL_TYPE + " TEXT, "
        + "PRIMARY KEY (" + COL_ACCOUNT_GUID + ", " + COL_PROFILE + "))";
    db.execSQL(createTableSql);
  }

  public void wipe() {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    onUpgrade(db, SCHEMA_VERSION, SCHEMA_VERSION);
  }

  @Override
  public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
    // For now we'll just drop and recreate the tables.
    db.execSQL("DROP TABLE IF EXISTS " + TBL_CLIENTS);
    onCreate(db);
  }

  // If a record with given GUID exists, we'll update it,
  // otherwise we'll insert it.
  public void store(String profileId, ClientRecord record) {
    SQLiteDatabase db = this.getCachedWritableDatabase();

    ContentValues cv = new ContentValues();
    cv.put(COL_ACCOUNT_GUID, record.guid);
    cv.put(COL_PROFILE, profileId);
    cv.put(COL_NAME, record.name);
    cv.put(COL_TYPE, record.type);

    String[] args = new String[] { record.guid, profileId };
    int rowsUpdated = db.update(TBL_CLIENTS, cv, TBL_KEY, args);

    if (rowsUpdated >= 1) {
      Logger.debug(LOG_TAG, "Replaced client record for row with accountGUID " + record.guid);
    } else {
      long rowId = db.insert(TBL_CLIENTS, null, cv);
      Logger.debug(LOG_TAG, "Inserted client record into row: " + rowId);
    }
  }

  public Cursor fetch(String accountGuid, String profileId) throws NullCursorException {
    String[] args = new String[] { accountGuid, profileId };
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetch", TBL_CLIENTS, TBL_COLUMNS, TBL_KEY, args);
  }

  public Cursor fetchAll() throws NullCursorException {
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetch", TBL_CLIENTS, TBL_COLUMNS, null, null);
  }

  public void delete(String accountGUID, String profileId) {
    String[] args = new String[] { accountGUID, profileId };

    SQLiteDatabase db = this.getCachedWritableDatabase();
    db.delete(TBL_CLIENTS, TBL_KEY, args);
  }
}
