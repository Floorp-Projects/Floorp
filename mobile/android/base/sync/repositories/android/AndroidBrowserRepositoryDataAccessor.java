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
 * Jason Voll <jvoll@mozilla.com>
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

import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

public abstract class AndroidBrowserRepositoryDataAccessor {

  protected Context context;
  protected String LOG_TAG = "AndroidBrowserRepositoryDataAccessor";

  public AndroidBrowserRepositoryDataAccessor(Context context) {
    this.context = context;
  }

  protected abstract String[] getAllColumns();
  protected abstract ContentValues getContentValues(Record record);
  protected abstract Uri getUri();
  protected long queryStart = 0;
  protected long queryEnd = 0;
  
  public void wipe() {
    Log.i(LOG_TAG, "wiping: " + getUri());
    String where = BrowserContract.SyncColumns.GUID + " NOT IN ('mobile')";
    context.getContentResolver().delete(getUri(), where, null);
  }
  
  public void purgeDeleted() throws NullCursorException {
    queryStart = System.currentTimeMillis();
    Cursor cur = context.getContentResolver().query(getUri(),
        new String[] { BrowserContract.SyncColumns.GUID },
        BrowserContract.SyncColumns.IS_DELETED + "= 1", null, null);
    queryEnd = System.currentTimeMillis();
    RepoUtils.queryTimeLogger(LOG_TAG + ".purgeDeleted", queryStart, queryEnd);
    if (cur == null) {
      Log.e(LOG_TAG, "Got back a null cursor in AndroidBrowserRepositoryDataAccessor.purgeDeleted");
      throw new NullCursorException(null);
    }
    cur.moveToFirst();
    while (!cur.isAfterLast()) {
      delete(RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID));
      cur.moveToNext();
    }
    cur.close();
  }
  
  protected void delete(String guid) {
    context.getContentResolver().delete(getUri(), BrowserContract.SyncColumns.GUID + " = '" + guid + "'", null);
  }

  public Uri insert(Record record) {
    ContentValues cv = getContentValues(record);
    return context.getContentResolver().insert(getUri(), cv);
  }
  
  public Cursor fetchAll() throws NullCursorException {
    queryStart = System.currentTimeMillis();
    Cursor cur = context.getContentResolver().query(getUri(),
        getAllColumns(), null, null, null);
    queryEnd = System.currentTimeMillis();
    
    RepoUtils.queryTimeLogger(LOG_TAG + ".fetchAll", queryStart, queryEnd);
    if (cur == null) {
      Log.e(LOG_TAG, "Got null cursor exception in AndroidBrowserRepositoryDataAccessor.fetchAll");
      throw new NullCursorException(null);
    }
    return cur;
  }
  
  public Cursor getGUIDsSince(long timestamp) throws NullCursorException {
    queryStart = System.currentTimeMillis();
    Cursor cur = context.getContentResolver().query(getUri(),
        new String[] { BrowserContract.SyncColumns.GUID },
        BrowserContract.SyncColumns.DATE_MODIFIED + " >= " +
        Long.toString(timestamp), null, null);
    queryEnd = System.currentTimeMillis();
    RepoUtils.queryTimeLogger(LOG_TAG + ".getGUIDsSince", queryStart, queryEnd);
    if (cur == null) {
      Log.e(LOG_TAG, "Got null cursor exception in AndroidBrowserRepositoryDataAccessor.getGUIDsSince");
      throw new NullCursorException(null);
    }
    return cur;
  }

  public Cursor fetchSince(long timestamp) throws NullCursorException {
    queryStart = System.currentTimeMillis();
    Cursor cur = context.getContentResolver().query(getUri(),
        getAllColumns(),
        BrowserContract.SyncColumns.DATE_MODIFIED + " >= " +
        Long.toString(timestamp), null, null);
    queryEnd = System.currentTimeMillis();
    RepoUtils.queryTimeLogger(LOG_TAG + ".fetchSince", queryStart, queryEnd);
    if (cur == null) {
      Log.e(LOG_TAG, "Got null cursor exception in AndroidBrowserRepositoryDataAccessor.fetchSince");
      throw new NullCursorException(null);
    }
    return cur;
  }

  public Cursor fetch(String guids[]) throws NullCursorException {
    String where = "guid" + " in (";
    for (String guid : guids) {
      where = where + "'" + guid + "', ";
    }
    where = (where.substring(0, where.length() -2) + ")");
    queryStart = System.currentTimeMillis();
    Cursor cur = context.getContentResolver().query(getUri(), getAllColumns(), where, null, null);
    queryEnd = System.currentTimeMillis();
    RepoUtils.queryTimeLogger(LOG_TAG + ".fetch", queryStart, queryEnd);
    if (cur == null) {
      Log.e(LOG_TAG, "Got null cursor exception in AndroidBrowserRepositoryDataAccessor.fetch");
      throw new NullCursorException(null);
    }
    return cur;
  }

  public void delete(Record record) {
    context.getContentResolver().delete(getUri(),
         BrowserContract.SyncColumns.GUID + " = '" + record.guid +"'", null);
  }

  public void updateByGuid(String guid, ContentValues cv) {
    context.getContentResolver().update(getUri(), cv,
        BrowserContract.SyncColumns.GUID + " = '" + guid +"'", null);
  }
}
