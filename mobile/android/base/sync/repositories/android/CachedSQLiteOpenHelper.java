/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteDatabase.CursorFactory;
import android.database.sqlite.SQLiteOpenHelper;

public abstract class CachedSQLiteOpenHelper extends SQLiteOpenHelper {

  public CachedSQLiteOpenHelper(Context context, String name, CursorFactory factory,
      int version) {
    super(context, name, factory, version);
  }

  // Cache these so we don't have to track them across cursors. Call `close`
  // when you're done.
  private static SQLiteDatabase readableDatabase;
  private static SQLiteDatabase writableDatabase;

  synchronized protected SQLiteDatabase getCachedReadableDatabase() {
    if (readableDatabase == null) {
      if (writableDatabase == null) {
        readableDatabase = this.getReadableDatabase();
        return readableDatabase;
      } else {
        return writableDatabase;
      }
    } else {
      return readableDatabase;
    }
  }

  synchronized protected SQLiteDatabase getCachedWritableDatabase() {
    if (writableDatabase == null) {
      writableDatabase = this.getWritableDatabase();
    }
    return writableDatabase;
  }

  synchronized public void close() {
    if (readableDatabase != null) {
      readableDatabase.close();
      readableDatabase = null;
    }
    if (writableDatabase != null) {
      writableDatabase.close();
      writableDatabase = null;
    }
    super.close();
  }
}
