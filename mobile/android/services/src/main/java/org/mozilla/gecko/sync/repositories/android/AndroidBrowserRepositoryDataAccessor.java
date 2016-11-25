/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.List;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.db.CursorDumper;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public abstract class AndroidBrowserRepositoryDataAccessor {

  private static final String[] GUID_COLUMNS = new String[] { BrowserContract.SyncColumns.GUID };
  protected Context context;
  protected static final String LOG_TAG = "BrowserDataAccessor";
  protected final RepoUtils.QueryHelper queryHelper;

  public AndroidBrowserRepositoryDataAccessor(Context context) {
    this.context = context;
    this.queryHelper = new RepoUtils.QueryHelper(context, getUri(), LOG_TAG);
  }

  protected abstract String[] getAllColumns();

  /**
   * Produce a <code>ContentValues</code> instance that represents the provided <code>Record</code>.
   *
   * @param record The <code>Record</code> to be converted.
   * @return The <code>ContentValues</code> corresponding to <code>record</code>.
   */
  protected abstract ContentValues getContentValues(Record record);

  protected abstract Uri getUri();

  /**
   * Dump all the records in raw format.
   */
  public void dumpDB() {
    Cursor cur = null;
    try {
      cur = queryHelper.safeQuery(".dumpDB", null, null, null, null);
      CursorDumper.dumpCursor(cur);
    } catch (NullCursorException e) {
    } finally {
      if (cur != null) {
        cur.close();
      }
    }
  }

  public String dateModifiedWhere(long timestamp) {
    return BrowserContract.SyncColumns.DATE_MODIFIED + " >= " + Long.toString(timestamp);
  }

  public void delete(String where, String[] args) {
    Uri uri = getUri();
    context.getContentResolver().delete(uri, where, args);
  }

  public void wipe() {
    Logger.debug(LOG_TAG, "Wiping.");
    delete(null, null);
  }

  public void purgeDeleted() throws NullCursorException {
    String where = BrowserContract.SyncColumns.IS_DELETED + "= 1";
    Uri uri = getUri();
    Logger.info(LOG_TAG, "Purging deleted from: " + uri);
    context.getContentResolver().delete(uri, where, null);
  }

  /**
   * Remove matching records from the database entirely, i.e., do not set a
   * deleted flag, delete entirely.
   *
   * @param guid
   *          The GUID of the record to be deleted.
   * @return The number of records deleted.
   */
  public int purgeGuid(String guid) {
    String where  = BrowserContract.SyncColumns.GUID + " = ?";
    String[] args = new String[] { guid };

    int deleted = context.getContentResolver().delete(getUri(), where, args);
    if (deleted != 1) {
      Logger.warn(LOG_TAG, "Unexpectedly deleted " + deleted + " records for guid " + guid);
    }
    return deleted;
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
   * <p>
   * The caller is responsible for closing the cursor.
   *
   * @return A cursor. You </b>must</b> close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor fetchAll() throws NullCursorException {
    return queryHelper.safeQuery(".fetchAll", getAllColumns(), null, null, null);
  }

  /**
   * Fetch GUIDs for records modified since the provided timestamp.
   * <p>
   * The caller is responsible for closing the cursor.
   *
   * @param timestamp A timestamp in milliseconds.
   * @return A cursor. You <b>must</b> close this when you're done with it.
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
   * <p>
   * The caller is responsible for closing the cursor.
   *
   * @param timestamp A timestamp in milliseconds.
   * @return A cursor. You <b>must</b> close this when you're done with it.
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
   * <p>
   * The caller is responsible for closing the cursor.
   *
   * @param guids The GUIDs of the records to fetch.
   * @return A cursor. You <b>must</b> close this when you're done with it.
   * @throws NullCursorException
   */
  public Cursor fetch(String guids[]) throws NullCursorException {
    String where = RepoUtils.computeSQLInClause(guids.length, "guid");
    return queryHelper.safeQuery(".fetch", getAllColumns(), where, guids, null);
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

  /**
   * Insert records.
   * <p>
   * This inserts all the records (using <code>ContentProvider.bulkInsert</code>),
   * but does <b>not</b> update the <code>androidID</code> of each record.
   *
   * @param records
   *          the records to insert.
   * @return
   *          the number of records actually inserted.
   * @throws NullCursorException
   */
  public int bulkInsert(List<Record> records) throws NullCursorException {
    if (records.isEmpty()) {
      Logger.debug(LOG_TAG, "No records to insert, returning.");
    }

    int size = records.size();
    ContentValues[] cvs = new ContentValues[size];
    int index = 0;
    for (Record record : records) {
      try {
        cvs[index] = getContentValues(record);
        index += 1;
      } catch (Exception e) {
        Logger.warn(LOG_TAG, "Got exception in getContentValues for record with guid " + record.guid, e);
      }
    }

    if (index != size) {
      // bulkInsert treats null ContentValues as blank rows, which we don't want
      // to insert into the database.
      // We expect exceptions in getContentValues to be exceedingly rare, so we
      // re-allocate in the (rare) error case and maintain a fast path for the
      // success case.
      size = index;
    }

    int inserted = context.getContentResolver().bulkInsert(getUri(), cvs);
    if (inserted == size) {
      Logger.debug(LOG_TAG, "Inserted " + inserted + " records, as expected.");
    } else {
      Logger.debug(LOG_TAG, "Inserted " +
                   inserted + " records but expected " +
                   size     + " records.");
    }
    return inserted;
  }
}
