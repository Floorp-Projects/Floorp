/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import java.util.ArrayList;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;

public class HistoryDataAccessor extends
        DataAccessor {

  public HistoryDataAccessor(Context context) {
    super(context);
  }

  @Override
  protected Uri getUri() {
    return BrowserContractHelpers.HISTORY_CONTENT_URI;
  }

  @Override
  protected ContentValues getContentValues(Record record) {
    // NB: these two sets of values (with or without visit information) must agree with the
    // BrowserProvider#bulkInsertHistory implementation.
    final ContentValues cv = new ContentValues();
    final HistoryRecord rec = (HistoryRecord) record;
    cv.put(BrowserContract.History.GUID, rec.guid);
    cv.put(BrowserContract.History.TITLE, rec.title);
    cv.put(BrowserContract.History.URL, rec.histURI);
    if (rec.visits != null) {
      final JSONArray visits = rec.visits;
      final long mostRecent = getLastVisited(visits);

      // Fennec stores history timestamps in milliseconds, and visit timestamps in microseconds.
      // The rest of Sync works in microseconds. This is the conversion point for records coming form Sync.
      cv.put(BrowserContract.History.DATE_LAST_VISITED, mostRecent / 1000);
      cv.put(BrowserContract.History.REMOTE_DATE_LAST_VISITED, mostRecent / 1000);
      cv.put(BrowserContract.History.VISITS, visits.size());
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

    // TODO this could use BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC, both for
    // speed (one transaction instead of one), and for the benefit of data consistency concerns.
    Logger.debug(LOG_TAG, "Storing record " + record.guid);
    Uri newRecordUri = super.insert(record);

    Logger.debug(LOG_TAG, "Storing visits for " + record.guid);
    context.getContentResolver().bulkInsert(
            BrowserContract.Visits.CONTENT_URI,
            VisitsHelper.getVisitsContentValues(rec.guid, rec.visits)
    );

    return newRecordUri;
  }

  /**
   * Given oldGUID, first updates corresponding history record with new values (super operation),
   * and then inserts visits from the new record.
   * Existing visits from the old record are updated on database level to point to new GUID if necessary.
   *
   * @param oldGUID GUID of old <code>HistoryRecord</code>
   * @param newRecord new <code>HistoryRecord</code> to replace old one with, and insert visits from
   */
  @Override
  public void update(String oldGUID, Record newRecord) {
    // First, update existing history records with new values. This might involve changing history GUID,
    // and thanks to ON UPDATE CASCADE clause on Visits.HISTORY_GUID foreign key, visits will be "ported over"
    // to the new GUID.
    super.update(oldGUID, newRecord);

    // Now we need to insert any visits from the new record
    HistoryRecord rec = (HistoryRecord) newRecord;
    String newGUID = newRecord.guid;
    Logger.debug(LOG_TAG, "Storing visits for " + newGUID + ", replacing " + oldGUID);

    context.getContentResolver().bulkInsert(
            BrowserContract.Visits.CONTENT_URI,
            VisitsHelper.getVisitsContentValues(newGUID, rec.visits)
    );
  }

  /**
   * Insert records.
   * <p>
   * This inserts all the records and their visit information using a custom ContentProvider interface.
   * Underlying ContentProvider must handle "call" method {@link BrowserContract#METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC}.
   *
   * @param records
   *          the records to insert.
   * @return
   *          the number of records actually inserted.
   * @throws NullCursorException
   */
  public boolean bulkInsert(ArrayList<HistoryRecord> records) throws NullCursorException {
    final Bundle[] historyBundles = new Bundle[records.size()];
    int i = 0;
    for (HistoryRecord record : records) {
      if (record.guid == null) {
        throw new IllegalArgumentException("Record with null GUID passed into bulkInsert.");
      }
      final Bundle historyBundle = new Bundle();
      historyBundle.putParcelable(BrowserContract.METHOD_PARAM_OBJECT, getContentValues(record));
      historyBundle.putSerializable(
              BrowserContract.History.VISITS,
              VisitsHelper.getVisitsContentValues(record.guid, record.visits)
      );
      historyBundles[i] = historyBundle;
      i++;
    }

    final Bundle data = new Bundle();
    data.putSerializable(BrowserContract.METHOD_PARAM_DATA, historyBundles);

    // Let our ContentProvider handle insertion of everything.
    final Bundle result = context.getContentResolver().call(
            getUri(),
            BrowserContract.METHOD_INSERT_HISTORY_WITH_VISITS_FROM_SYNC,
            getUri().toString(),
            data
    );
    if (result == null) {
      throw new IllegalStateException("Unexpected null result while bulk inserting history");
    }
    final Exception thrownException = (Exception) result.getSerializable(BrowserContract.METHOD_RESULT);
    return thrownException == null;
  }

  /**
   * Helper method used to find largest <code>VisitsHelper.SYNC_DATE_KEY</code> value in a provided JSONArray.
   *
   * @param visits Array of objects which will be searched.
   * @return largest value of <code>VisitsHelper.SYNC_DATE_KEY</code>.
     */
  private long getLastVisited(JSONArray visits) {
    long mostRecent = 0;
    for (int i = 0; i < visits.size(); i++) {
      final JSONObject visit = (JSONObject) visits.get(i);
      long visitDate = (Long) visit.get(VisitsHelper.SYNC_DATE_KEY);
      if (visitDate > mostRecent) {
        mostRecent = visitDate;
      }
    }
    return mostRecent;
  }
}
