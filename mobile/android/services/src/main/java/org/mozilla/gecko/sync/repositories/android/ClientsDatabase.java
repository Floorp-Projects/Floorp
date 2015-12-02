/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.background.common.log.Logger;
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
  protected static final int SCHEMA_VERSION = 3;

  // Clients Table.
  public static final String TBL_CLIENTS      = "clients";
  public static final String COL_ACCOUNT_GUID = "guid";
  public static final String COL_PROFILE      = "profile";
  public static final String COL_NAME         = "name";
  public static final String COL_TYPE         = "device_type";

  // Optional fields.
  public static final String COL_FORMFACTOR = "formfactor";
  public static final String COL_OS = "os";
  public static final String COL_APPLICATION = "application";
  public static final String COL_APP_PACKAGE = "appPackage";
  public static final String COL_DEVICE = "device";

  public static final String[] TBL_CLIENTS_COLUMNS = new String[] { COL_ACCOUNT_GUID, COL_PROFILE, COL_NAME, COL_TYPE,
                                                                    COL_FORMFACTOR, COL_OS, COL_APPLICATION, COL_APP_PACKAGE, COL_DEVICE };
  public static final String TBL_CLIENTS_KEY = COL_ACCOUNT_GUID + " = ? AND " +
                                               COL_PROFILE + " = ?";

  // Commands Table.
  public static final String TBL_COMMANDS = "commands";
  public static final String COL_COMMAND  = "command";
  public static final String COL_ARGS     = "args";

  public static final String[] TBL_COMMANDS_COLUMNS    = new String[] { COL_ACCOUNT_GUID, COL_COMMAND, COL_ARGS };
  public static final String   TBL_COMMANDS_KEY        = COL_ACCOUNT_GUID + " = ? AND " +
                                                         COL_COMMAND + " = ? AND " +
                                                         COL_ARGS + " = ?";
  public static final String   TBL_COMMANDS_GUID_QUERY = COL_ACCOUNT_GUID + " = ? ";

  private final RepoUtils.QueryHelper queryHelper;

  public ClientsDatabase(Context context) {
    super(context, DB_NAME, null, SCHEMA_VERSION);
    this.queryHelper = new RepoUtils.QueryHelper(context, null, LOG_TAG);
    Logger.debug(LOG_TAG, "ClientsDatabase instantiated.");
  }

  @Override
  public void onCreate(SQLiteDatabase db) {
    Logger.debug(LOG_TAG, "ClientsDatabase.onCreate().");
    createClientsTable(db);
    createCommandsTable(db);
  }

  public static void createClientsTable(SQLiteDatabase db) {
    Logger.debug(LOG_TAG, "ClientsDatabase.createClientsTable().");
    String createClientsTableSql = "CREATE TABLE " + TBL_CLIENTS + " ("
        + COL_ACCOUNT_GUID + " TEXT, "
        + COL_PROFILE + " TEXT, "
        + COL_NAME + " TEXT, "
        + COL_TYPE + " TEXT, "
        + COL_FORMFACTOR + " TEXT, "
        + COL_OS + " TEXT, "
        + COL_APPLICATION + " TEXT, "
        + COL_APP_PACKAGE + " TEXT, "
        + COL_DEVICE + " TEXT, "
        + "PRIMARY KEY (" + COL_ACCOUNT_GUID + ", " + COL_PROFILE + "))";
    db.execSQL(createClientsTableSql);
  }

  public static void createCommandsTable(SQLiteDatabase db) {
    Logger.debug(LOG_TAG, "ClientsDatabase.createCommandsTable().");
    String createCommandsTableSql = "CREATE TABLE " + TBL_COMMANDS + " ("
        + COL_ACCOUNT_GUID + " TEXT, "
        + COL_COMMAND + " TEXT, "
        + COL_ARGS + " TEXT, "
        + "PRIMARY KEY (" + COL_ACCOUNT_GUID + ", " + COL_COMMAND + ", " + COL_ARGS + "), "
        + "FOREIGN KEY (" + COL_ACCOUNT_GUID + ") REFERENCES " + TBL_CLIENTS + " (" + COL_ACCOUNT_GUID + "))";
    db.execSQL(createCommandsTableSql);
  }

  @Override
  public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
    Logger.debug(LOG_TAG, "ClientsDatabase.onUpgrade(" + oldVersion + ", " + newVersion + ").");
    if (oldVersion < 2) {
      // For now we'll just drop and recreate the tables.
      db.execSQL("DROP TABLE IF EXISTS " + TBL_CLIENTS);
      db.execSQL("DROP TABLE IF EXISTS " + TBL_COMMANDS);
      onCreate(db);
      return;
    }

    if (newVersion >= 3) {
      // Add the optional columns to clients.
      db.execSQL("ALTER TABLE " + TBL_CLIENTS + " ADD COLUMN " + COL_FORMFACTOR + " TEXT");
      db.execSQL("ALTER TABLE " + TBL_CLIENTS + " ADD COLUMN " + COL_OS + " TEXT");
      db.execSQL("ALTER TABLE " + TBL_CLIENTS + " ADD COLUMN " + COL_APPLICATION + " TEXT");
      db.execSQL("ALTER TABLE " + TBL_CLIENTS + " ADD COLUMN " + COL_APP_PACKAGE + " TEXT");
      db.execSQL("ALTER TABLE " + TBL_CLIENTS + " ADD COLUMN " + COL_DEVICE + " TEXT");
    }
  }

  public void wipeDB() {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    onUpgrade(db, 0, SCHEMA_VERSION);
  }

  public void wipeClientsTable() {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    db.execSQL("DELETE FROM " + TBL_CLIENTS);
  }

  public void wipeCommandsTable() {
    SQLiteDatabase db = this.getCachedWritableDatabase();
    db.execSQL("DELETE FROM " + TBL_COMMANDS);
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

    if (record.formfactor != null) {
      cv.put(COL_FORMFACTOR, record.formfactor);
    }

    if (record.os != null) {
      cv.put(COL_OS, record.os);
    }

    if (record.application != null) {
      cv.put(COL_APPLICATION, record.application);
    }

    if (record.appPackage != null) {
      cv.put(COL_APP_PACKAGE, record.appPackage);
    }

    if (record.device != null) {
      cv.put(COL_DEVICE, record.device);
    }

    String[] args = new String[] { record.guid, profileId };
    int rowsUpdated = db.update(TBL_CLIENTS, cv, TBL_CLIENTS_KEY, args);

    if (rowsUpdated >= 1) {
      Logger.debug(LOG_TAG, "Replaced client record for row with accountGUID " + record.guid);
    } else {
      long rowId = db.insert(TBL_CLIENTS, null, cv);
      Logger.debug(LOG_TAG, "Inserted client record into row: " + rowId);
    }
  }

  /**
   * Store a command in the commands database if it doesn't already exist.
   *
   * @param accountGUID
   * @param command - The command type
   * @param args - A JSON string of args
   * @throws NullCursorException
   */
  public void store(String accountGUID, String command, String args) throws NullCursorException {
    if (Logger.LOG_PERSONAL_INFORMATION) {
      Logger.pii(LOG_TAG, "Storing command " + command + " with args " + args);
    } else {
      Logger.trace(LOG_TAG, "Storing command " + command + ".");
    }
    SQLiteDatabase db = this.getCachedWritableDatabase();

    ContentValues cv = new ContentValues();
    cv.put(COL_ACCOUNT_GUID, accountGUID);
    cv.put(COL_COMMAND, command);
    if (args == null) {
      cv.put(COL_ARGS, "[]");
    } else {
      cv.put(COL_ARGS, args);
    }

    Cursor cur = this.fetchSpecificCommand(accountGUID, command, args);
    try {
      if (cur.moveToFirst()) {
        Logger.debug(LOG_TAG, "Command already exists in database.");
        return;
      }
    } finally {
      cur.close();
    }

    long rowId = db.insert(TBL_COMMANDS, null, cv);
    Logger.debug(LOG_TAG, "Inserted command into row: " + rowId);
  }

  public Cursor fetchClientsCursor(String accountGUID, String profileId) throws NullCursorException {
    String[] args = new String[] { accountGUID, profileId };
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetchClientsCursor", TBL_CLIENTS, TBL_CLIENTS_COLUMNS, TBL_CLIENTS_KEY, args);
  }

  public Cursor fetchSpecificCommand(String accountGUID, String command, String commandArgs) throws NullCursorException {
    String[] args = new String[] { accountGUID, command, commandArgs };
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetchSpecificCommand", TBL_COMMANDS, TBL_COMMANDS_COLUMNS, TBL_COMMANDS_KEY, args);
  }

  public Cursor fetchCommandsForClient(String accountGUID) throws NullCursorException {
    String[] args = new String[] { accountGUID };
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetchCommandsForClient", TBL_COMMANDS, TBL_COMMANDS_COLUMNS, TBL_COMMANDS_GUID_QUERY, args);
  }

  public Cursor fetchAllClients() throws NullCursorException {
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetchAllClients", TBL_CLIENTS, TBL_CLIENTS_COLUMNS, null, null);
  }

  public Cursor fetchAllCommands() throws NullCursorException {
    SQLiteDatabase db = this.getCachedReadableDatabase();

    return queryHelper.safeQuery(db, ".fetchAllCommands", TBL_COMMANDS, TBL_COMMANDS_COLUMNS, null, null);
  }

  public void deleteClient(String accountGUID, String profileId) {
    String[] args = new String[] { accountGUID, profileId };

    SQLiteDatabase db = this.getCachedWritableDatabase();
    db.delete(TBL_CLIENTS, TBL_CLIENTS_KEY, args);
  }
}
