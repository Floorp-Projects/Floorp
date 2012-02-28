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

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public abstract class AndroidBrowserRepositoryDataAccessor {

  private static final String[] GUID_COLUMNS = new String[] { BrowserContract.SyncColumns.GUID };
  protected Context context;
  protected static String LOG_TAG = "BrowserDataAccessor";
  private final RepoUtils.QueryHelper queryHelper;

  public AndroidBrowserRepositoryDataAccessor(Context context) {
    this.context = context;
    this.queryHelper = new RepoUtils.QueryHelper(context, getUri(), LOG_TAG);
  }

  protected abstract String[] getAllColumns();
  protected abstract ContentValues getContentValues(Record record);
  protected abstract Uri getUri();

  public String dateModifiedWhere(long timestamp) {
    return BrowserContract.SyncColumns.DATE_MODIFIED + " >= " + Long.toString(timestamp);
  }

  public void wipe() {
    Logger.info(LOG_TAG, "wiping: " + getUri());
    String where = BrowserContract.SyncColumns.GUID + " NOT IN ('mobile')";
    context.getContentResolver().delete(getUri(), where, null);
  }
  
  public void purgeDeleted() throws NullCursorException {
    String where = BrowserContract.SyncColumns.IS_DELETED + "= 1";
    Cursor cur = queryHelper.safeQuery(".purgeDeleted", GUID_COLUMNS, where, null, null);

    try {
      if (!cur.moveToFirst()) {
        return;
      }
      while (!cur.isAfterLast()) {
        delete(RepoUtils.getStringFromCursor(cur, BrowserContract.SyncColumns.GUID));
        cur.moveToNext();
      }
    } finally {
      cur.close();
    }
  }
  
  protected void delete(String guid) {
    String where  = BrowserContract.SyncColumns.GUID + " = ?";
    String[] args = new String[] { guid };

    int deleted = context.getContentResolver().delete(getUri(), where, args);
    if (deleted == 1) {
      return;
    }
    Logger.warn(LOG_TAG, "Unexpectedly deleted " + deleted + " rows for guid " + guid);
  }

  public void update(String guid, Record newRecord) {
    String where  = BrowserContract.SyncColumns.GUID + " = ?";
    String[] args = new String[] { guid };
    ContentValues cv = getContentValues(newRecord);
    int updated = context.getContentResolver().update(getUri(), cv, where, args);
    if (updated != 1) {
      Logger.warn(LOG_TAG, "Unexpectedly updated " + updated + " rows for guid " + guid);
    }
  }

  public Uri insert(Record record) {
    ContentValues cv = getContentValues(record);
    return context.getContentResolver().insert(getUri(), cv);
  }

  /**
   * Fetch all records.
   * The caller is responsible for closing the cursor.
   *
   * @return A cursor. You *must* close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor fetchAll() throws NullCursorException {
    return queryHelper.safeQuery(".fetchAll", getAllColumns(), null, null, null);
  }
  
  /**
   * Fetch GUIDs for records modified since the provided timestamp.
   * The caller is responsible for closing the cursor.
   *
   * @param timestamp
   * @return A cursor. You *must* close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor getGUIDsSince(long timestamp) throws NullCursorException {
    return queryHelper.safeQuery(".getGUIDsSince",
                                 GUID_COLUMNS,
                                 dateModifiedWhere(timestamp),
                                 null, null);
  }

  /**
   * Fetch records modified since the provided timestamp.
   * The caller is responsible for closing the cursor.
   *
   * @param timestamp
   * @return A cursor. You *must* close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor fetchSince(long timestamp) throws NullCursorException {
    return queryHelper.safeQuery(".fetchSince",
                                 getAllColumns(),
                                 dateModifiedWhere(timestamp),
                                 null, null);
  }

  /**
   * Fetch records for the provided GUIDs.
   * The caller is responsible for closing the cursor.
   *
   * @param guids
   * @return A cursor. You *must* close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor fetch(String guids[]) throws NullCursorException {
    String where = computeSQLInClause(guids.length, "guid");
    return queryHelper.safeQuery(".fetch", getAllColumns(), where, guids, null);
  }

  protected String computeSQLInClause(int items, String field) {
    StringBuilder builder = new StringBuilder(field);
    builder.append(" IN (");
    int i = 0;
    for (; i < items - 1; ++i) {
      builder.append("?, ");
    }
    if (i < items) {
      builder.append("?");
    }
    builder.append(")");
    return builder.toString();
  }

  public void delete(Record record) {
    String where  = BrowserContract.SyncColumns.GUID + " = ?";
    String[] args = new String[] { record.guid };

    int deleted = context.getContentResolver().delete(getUri(), where, args);
    if (deleted == 1) {
      return;
    }
    Logger.warn(LOG_TAG, "Unexpectedly deleted " + deleted + " rows for guid " + record.guid);
  }

  public void updateByGuid(String guid, ContentValues cv) {
    String where  = BrowserContract.SyncColumns.GUID + " = ?";
    String[] args = new String[] { guid };

    int updated = context.getContentResolver().update(getUri(), cv, where, args);
    if (updated == 1) {
      return;
    }
    Logger.warn(LOG_TAG, "Unexpectedly updated " + updated + " rows for guid " + guid);
  }
}
