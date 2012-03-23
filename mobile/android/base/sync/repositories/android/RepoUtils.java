/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.json.simple.JSONArray;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.PasswordRecord;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.net.Uri;

public class RepoUtils {

  private static final String LOG_TAG = "RepoUtils";

  /**
   * A helper class for monotonous SQL querying. Does timing and logging,
   * offers a utility to throw on a null cursor.
   *
   * @author rnewman
   *
   */
  public static class QueryHelper {
    private final Context context;
    private final Uri     uri;
    private final String  tag;

    public QueryHelper(Context context, Uri uri, String tag) {
      this.context = context;
      this.uri     = uri;
      this.tag     = tag;
    }

    // For ContentProvider queries.
    public Cursor safeQuery(String label, String[] projection,
                            String selection, String[] selectionArgs, String sortOrder) throws NullCursorException {
      long queryStart = android.os.SystemClock.uptimeMillis();
      Cursor c = context.getContentResolver().query(uri, projection, selection, selectionArgs, sortOrder);
      return checkAndLogCursor(label, queryStart, c);
    }

    public Cursor safeQuery(String[] projection, String selection, String[] selectionArgs, String sortOrder) throws NullCursorException {
      return this.safeQuery(null, projection, selection, selectionArgs, sortOrder);
    }

    // For SQLiteOpenHelper queries.
    public Cursor safeQuery(SQLiteDatabase db, String label, String table, String[] columns,
                            String selection, String[] selectionArgs,
                            String groupBy, String having, String orderBy, String limit) throws NullCursorException {
      long queryStart = android.os.SystemClock.uptimeMillis();
      Cursor c = db.query(table, columns, selection, selectionArgs, groupBy, having, orderBy, limit);
      return checkAndLogCursor(label, queryStart, c);
    }

    public Cursor safeQuery(SQLiteDatabase db, String label, String table, String[] columns,
                            String selection, String[] selectionArgs) throws NullCursorException {
      return safeQuery(db, label, table, columns, selection, selectionArgs, null, null, null, null);
    }

    private Cursor checkAndLogCursor(String label, long queryStart, Cursor c) throws NullCursorException {
      long queryEnd = android.os.SystemClock.uptimeMillis();
      String logLabel = (label == null) ? tag : (tag + label);
      RepoUtils.queryTimeLogger(logLabel, queryStart, queryEnd);
      return checkNullCursor(logLabel, c);
    }

    public Cursor checkNullCursor(String logLabel, Cursor cursor) throws NullCursorException {
      if (cursor == null) {
        Logger.error(tag, "Got null cursor exception in " + logLabel);
        throw new NullCursorException(null);
      }
      return cursor;
    }
  }

  public static String getStringFromCursor(Cursor cur, String colId) {
    // TODO: getColumnIndexOrThrow?
    // TODO: don't look up columns by name!
    return cur.getString(cur.getColumnIndex(colId));
  }

  public static long getLongFromCursor(Cursor cur, String colId) {
    return cur.getLong(cur.getColumnIndex(colId));
  }

  public static int getIntFromCursor(Cursor cur, String colId) {
    return cur.getInt(cur.getColumnIndex(colId));
  }

  public static JSONArray getJSONArrayFromCursor(Cursor cur, String colId) {
    String jsonArrayAsString = getStringFromCursor(cur, colId);
    if (jsonArrayAsString == null) {
      return new JSONArray();
    }
    try {
      return (JSONArray) new JSONParser().parse(getStringFromCursor(cur, colId));
    } catch (ParseException e) {
      Logger.error(LOG_TAG, "JSON parsing error for " + colId, e);
      return null;
    }
  }

  // Returns android id from the URI that we get after inserting a
  // bookmark into the local Android store.
  public static long getAndroidIdFromUri(Uri uri) {
    String path = uri.getPath();
    int lastSlash = path.lastIndexOf('/');
    return Long.parseLong(path.substring(lastSlash + 1));
  }

  //Create a HistoryRecord object from a cursor on a row with a Moz History record in it
  public static HistoryRecord historyFromMirrorCursor(Cursor cur) {

    String guid = getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
    String collection = "history";
    long lastModified = getLongFromCursor(cur, BrowserContract.SyncColumns.DATE_MODIFIED);
    boolean deleted = getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) == 1 ? true : false;
    HistoryRecord rec = new HistoryRecord(guid, collection, lastModified, deleted);

    rec.title = getStringFromCursor(cur, BrowserContract.History.TITLE);
    rec.histURI = getStringFromCursor(cur, BrowserContract.History.URL);
    rec.androidID = getLongFromCursor(cur, BrowserContract.History._ID);
    rec.fennecDateVisited = getLongFromCursor(cur, BrowserContract.History.DATE_LAST_VISITED);
    rec.fennecVisitCount = getLongFromCursor(cur, BrowserContract.History.VISITS);

    return logHistory(rec);
  }

  private static HistoryRecord logHistory(HistoryRecord rec) {
    try {
      Logger.debug(LOG_TAG, "Returning history record " + rec.guid + " (" + rec.androidID + ")");
      Logger.debug(LOG_TAG, "> Visited:          " + rec.fennecDateVisited);
      Logger.debug(LOG_TAG, "> Visits:           " + rec.fennecVisitCount);
      if (Logger.LOG_PERSONAL_INFORMATION) {
        Logger.pii(LOG_TAG, "> Title:            " + rec.title);
        Logger.pii(LOG_TAG, "> URI:              " + rec.histURI);
      }
    } catch (Exception e) {
      Logger.debug(LOG_TAG, "Exception logging history record " + rec, e);
    }
    return rec;
  }

  public static void logClient(ClientRecord rec) {
    if (Logger.logVerbose(LOG_TAG)) {
      Logger.trace(LOG_TAG, "Returning client record " + rec.guid + " (" + rec.androidID + ")");
      Logger.trace(LOG_TAG, "Client Name:   " + rec.name);
      Logger.trace(LOG_TAG, "Client Type:   " + rec.type);
      Logger.trace(LOG_TAG, "Last Modified: " + rec.lastModified);
      Logger.trace(LOG_TAG, "Deleted:       " + rec.deleted);
    }
  }

  public static PasswordRecord passwordFromMirrorCursor(Cursor cur) {
    
    String guid = getStringFromCursor(cur, BrowserContract.SyncColumns.GUID);
    String collection = "passwords";
    long lastModified = getLongFromCursor(cur, BrowserContract.SyncColumns.DATE_MODIFIED);
    boolean deleted = getLongFromCursor(cur, BrowserContract.SyncColumns.IS_DELETED) == 1 ? true : false;
    PasswordRecord rec = new PasswordRecord(guid, collection, lastModified, deleted);
    rec.hostname = getStringFromCursor(cur, BrowserContract.Passwords.HOSTNAME);
    rec.httpRealm = getStringFromCursor(cur, BrowserContract.Passwords.HTTP_REALM);
    rec.formSubmitURL = getStringFromCursor(cur, BrowserContract.Passwords.FORM_SUBMIT_URL);
    rec.usernameField = getStringFromCursor(cur, BrowserContract.Passwords.USERNAME_FIELD);
    rec.passwordField = getStringFromCursor(cur, BrowserContract.Passwords.PASSWORD_FIELD);
    rec.encType = getStringFromCursor(cur, BrowserContract.Passwords.ENC_TYPE);
    
    // TODO decryption of username/password here (Bug 711636)
    rec.username = getStringFromCursor(cur, BrowserContract.Passwords.ENCRYPTED_USERNAME);
    rec.password = getStringFromCursor(cur, BrowserContract.Passwords.ENCRYPTED_PASSWORD);
    
    rec.timeLastUsed = getLongFromCursor(cur, BrowserContract.Passwords.TIME_LAST_USED);
    rec.timesUsed = getLongFromCursor(cur, BrowserContract.Passwords.TIMES_USED);
    
    return rec;
  }
  
  public static void queryTimeLogger(String methodCallingQuery, long queryStart, long queryEnd) {
    long elapsedTime = queryEnd - queryStart;
    Logger.debug(LOG_TAG, "Query timer: " + methodCallingQuery + " took " + elapsedTime + "ms.");
  }

  public static boolean stringsEqual(String a, String b) {
    // Check for nulls
    if (a == b) return true;
    if (a == null && b != null) return false;
    if (a != null && b == null) return false;
    
    return a.equals(b);
  }

  private static String fixedWidth(int width, String s) {
    if (s == null) {
      return spaces(width);
    }
    int length = s.length();
    if (width == length) {
      return s;
    }
    if (width > length) {
      return s + spaces(width - length);
    }
    return s.substring(0, width);
  }

  private static String spaces(int i) {
    return "                                     ".substring(0, i);
  }

  private static String dashes(int i) {
    return "-------------------------------------".substring(0, i);
  }

  public static void dumpCursor(Cursor cur) {
    dumpCursor(cur, 18, "records");
  }

  public static void dumpCursor(Cursor cur, int columnWidth, String tag) {
    int originalPosition = cur.getPosition();
    try {
      String[] columnNames = cur.getColumnNames();
      int columnCount      = cur.getColumnCount();

      for (int i = 0; i < columnCount; ++i) {
        System.out.print(fixedWidth(columnWidth, columnNames[i]) + " | ");
      }
      System.out.println("(" + cur.getCount() + " " + tag + ")");
      for (int i = 0; i < columnCount; ++i) {
        System.out.print(dashes(columnWidth) + " | ");
      }
      System.out.println("");
      if (!cur.moveToFirst()) {
        System.out.println("EMPTY");
        return;
      }

      cur.moveToFirst();
      while (!cur.isAfterLast()) {
        for (int i = 0; i < columnCount; ++i) {
          System.out.print(fixedWidth(columnWidth, cur.getString(i)) + " | ");
        }
        System.out.println("");
        cur.moveToNext();
      }
      for (int i = 0; i < columnCount-1; ++i) {
        System.out.print(dashes(columnWidth + 3));
      }
      System.out.print(dashes(columnWidth + 3 - 1));
      System.out.println("");
    } finally {
      cur.moveToPosition(originalPosition);
    }
  }
}
