/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

public class AndroidBrowserHistoryDataAccessor extends
    AndroidBrowserRepositoryDataAccessor {

  private AndroidBrowserHistoryDataExtender dataExtender;

  public AndroidBrowserHistoryDataAccessor(Context context) {
    super(context);
    dataExtender = new AndroidBrowserHistoryDataExtender(context);
  }

  public AndroidBrowserHistoryDataExtender getHistoryDataExtender() {
    return dataExtender;
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.HISTORY_CONTENT_URI;
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    ContentValues cv = new ContentValues();
    HistoryRecord rec = (HistoryRecord) record;
    cv.put(BrowserContract.History.GUID, rec.guid);
    cv.put(BrowserContract.History.TITLE, rec.title);
    cv.put(BrowserContract.History.URL, rec.histURI);
    if (rec.visits != null) {
      JSONArray visits = rec.visits;
      long mostRecent = 0;
      for (int i = 0; i < visits.size(); i++) {
        JSONObject visit = (JSONObject) visits.get(i);
        long visitDate = (Long) visit
            .get(AndroidBrowserHistoryRepositorySession.KEY_DATE);
        if (visitDate > mostRecent) {
          mostRecent = visitDate;
        }
      }
      // Fennec stores milliseconds. The rest of Sync works in microseconds.
      cv.put(BrowserContract.History.DATE_LAST_VISITED, mostRecent / 1000);
      cv.put(BrowserContract.History.VISITS, Long.toString(visits.size()));
    }
    return cv;
  }

  @Override
  protected String[] getAllColumns() {
    return BrowserContractHelpers.HistoryColumns;
  }

  @Override
  public Uri insert(Record record) {
    HistoryRecord rec = (HistoryRecord) record;
    Logger.debug(LOG_TAG, "Storing visits for " + record.guid);
    dataExtender.store(record.guid, rec.visits);
    Logger.debug(LOG_TAG, "Storing record " + record.guid);
    return super.insert(record);
  }

  @Override
  public void update(String oldGUID, Record newRecord) {
    HistoryRecord rec = (HistoryRecord) newRecord;
    String newGUID = newRecord.guid;
    Logger.debug(LOG_TAG, "Storing visits for " + newGUID + ", replacing " + oldGUID);
    dataExtender.delete(oldGUID);
    dataExtender.store(newGUID, rec.visits);
    super.update(oldGUID, newRecord);
  }

  @Override
  public int purgeGuid(String guid) {
    Logger.debug(LOG_TAG, "Purging record with " + guid);
    dataExtender.delete(guid);
    return super.purgeGuid(guid);
  }

  public void closeExtender() {
    dataExtender.close();
  }

  public static String[] GUID_AND_ID = new String[] { BrowserContract.History.GUID, BrowserContract.History._ID };

  /**
   * Insert records.
   * <p>
   * This inserts all the records (using <code>ContentProvider.bulkInsert</code>),
   * then inserts all the visit information (using the data extender's
   * <code>bulkInsert</code>, which internally uses a single database
   * transaction), and then optionally updates the <code>androidID</code> of
   * each record.
   *
   * @param records
   *          The records to insert.
   * @param fetchFreshAndroidIDs
   *          <code>true</code> to update the <code>androidID</code> of each
   *          record; <code>false</code> to invalidate them all.
   * @throws NullCursorException
   */
  public void bulkInsert(ArrayList<HistoryRecord> records, boolean fetchFreshAndroidIDs) throws NullCursorException {
    if (records.isEmpty()) {
      Logger.debug(LOG_TAG, "No records to insert, returning.");
    }

    int size = records.size();
    ContentValues[] cvs = new ContentValues[size];
    String[] guids = new String[size];
    Map<String, Record> guidToRecord = new HashMap<String, Record>();
    int index = 0;
    for (Record record : records) {
      if (record.guid == null) {
        throw new IllegalArgumentException("Record with null GUID passed in to bulkInsert.");
      }
      cvs[index] = getContentValues(record);
      guids[index] = record.guid;
      guidToRecord.put(record.guid, record);
      index += 1;
    }

    // First update the history records.
    int inserted = context.getContentResolver().bulkInsert(getUri(), cvs);
    if (inserted == size) {
      Logger.debug(LOG_TAG, "Inserted " + inserted + " records, as expected.");
    } else {
      Logger.debug(LOG_TAG, "Inserted " +
                   inserted + " records but expected " +
                   size     + " records; continuing to update visits.");
    }
    // Then update the history visits.
    dataExtender.bulkInsert(records);

    // And finally patch up the androidIDs.
    if (!fetchFreshAndroidIDs) {
      return;
    }

    // We do this here to save a few loops.
    String guidIn = RepoUtils.computeSQLInClause(guids.length, BrowserContract.History.GUID);
    Cursor cursor = queryHelper.safeQuery("", GUID_AND_ID, guidIn, guids, null);
    int guidIndex = cursor.getColumnIndexOrThrow(BrowserContract.History.GUID);
    int androidIDIndex = cursor.getColumnIndexOrThrow(BrowserContract.History._ID);

    try {
      cursor.moveToFirst();
      while (!cursor.isAfterLast()) {
        String guid = cursor.getString(guidIndex);
        int androidID = cursor.getInt(androidIDIndex);
        cursor.moveToNext();

        Record record = guidToRecord.get(guid);
        if (record == null) {
          // Should never happen!
          Logger.warn(LOG_TAG, "Failed to update androidID for record with guid " + guid + ".");
          continue;
        }
        record.androidID = androidID;
      }
    } finally {
      if (cursor != null) {
        cursor.close();
      }
    }
  }
}
