/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.domain.HistoryRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;

import android.content.Context;
import android.database.Cursor;
import android.util.Log;

public class AndroidBrowserHistoryRepositorySession extends AndroidBrowserRepositorySession {
  
  public static final String KEY_DATE = "date";
  public static final String KEY_TYPE = "type";
  public static final long DEFAULT_VISIT_TYPE = 1;

  public AndroidBrowserHistoryRepositorySession(Repository repository, Context context) {
    super(repository);
    dbHelper = new AndroidBrowserHistoryDataAccessor(context);
  }

  @Override
  protected Record retrieveDuringStore(Cursor cur) {
    return RepoUtils.historyFromMirrorCursor(cur);
  }

  @Override
  protected Record retrieveDuringFetch(Cursor cur) {
    return RepoUtils.historyFromMirrorCursor(cur);
  }

  @Override
  protected String buildRecordString(Record record) {
    HistoryRecord hist = (HistoryRecord) record;
    return hist.title + hist.histURI;
  }

  @Override
  protected Record transformRecord(Record record) throws NullCursorException {
    return addVisitsToRecord(record);
  }

  @SuppressWarnings("unchecked")
  private void addVisit(JSONArray visits, long date, long visitType) {
    JSONObject visit = new JSONObject();
    visit.put(KEY_DATE, date);               // Microseconds since epoch.
    visit.put(KEY_TYPE, visitType);
    visits.add(visit);
  }

  private void addVisit(JSONArray visits, long date) {
    addVisit(visits, date, DEFAULT_VISIT_TYPE);
  }

  private AndroidBrowserHistoryDataExtender getDataExtender() {
    return ((AndroidBrowserHistoryDataAccessor) dbHelper).getHistoryDataExtender();
  }

  private Record addVisitsToRecord(Record record) throws NullCursorException {
    Log.d(LOG_TAG, "Adding visits for GUID " + record.guid);
    HistoryRecord hist = (HistoryRecord) record;
    JSONArray visitsArray = getDataExtender().visitsForGUID(hist.guid);
    long missingRecords = hist.fennecVisitCount - visitsArray.size();

    // Note that Fennec visit times are milliseconds, and we are working
    // in microseconds. This is the point at which we translate.

    // Add (missingRecords - 1) fake visits...
    if (missingRecords > 0) {
      long fakes = missingRecords - 1;
      for (int j = 0; j < fakes; j++) {
        // Set fake visit timestamp to be just previous to
        // the real one we are about to add.
        // TODO: make these equidistant?
        long fakeDate = (hist.fennecDateVisited - (1 + j)) * 1000;
        addVisit(visitsArray, fakeDate);
      }

      // ... and the 1 actual record we have.
      // We still have to fake the visit type: Fennec doesn't track that.
      addVisit(visitsArray, hist.fennecDateVisited * 1000);
    }

    hist.visits = visitsArray;
    return hist;
  }

  @Override
  protected Record prepareRecord(Record record) {
    return record;
  }
  
  @Override
  public void abort() {
    ((AndroidBrowserHistoryDataAccessor) dbHelper).closeExtender();
    super.abort();
  }

  @Override
  public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
    ((AndroidBrowserHistoryDataAccessor) dbHelper).closeExtender();
    super.finish(delegate);
  }
}
