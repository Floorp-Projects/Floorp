/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.support.annotation.NonNull;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.db.BrowserContract.Visits;

/**
 * This class is used by History Sync code (see <code>AndroidBrowserHistoryDataAccessor</code> and <code>AndroidBrowserHistoryRepositorySession</code>,
 * and provides utility functions for working with history visits. Primarily we're either inserting visits
 * into local database based on data received from Sync, or we're preparing local visits for upload into Sync.
 */
public class VisitsHelper {
    public static final boolean DEFAULT_IS_LOCAL_VALUE = false;
    public static final String SYNC_TYPE_KEY = "type";
    public static final String SYNC_DATE_KEY = "date";

    /**
     * Returns a list of ContentValues of visits ready for insertion for a provided History GUID.
     * Visits must have data and type. See <code>getVisitContentValues</code>.
     *
     * @param guid History GUID to use when inserting visit records
     * @param visits <code>JSONArray</code> list of (date, type) tuples for visits
     * @return visits ready for insertion
     */
    public static ContentValues[] getVisitsContentValues(@NonNull String guid, @NonNull JSONArray visits) {
        final ContentValues[] visitsToStore = new ContentValues[visits.size()];
        final int visitCount = visits.size();

        if (visitCount == 0) {
            return visitsToStore;
        }

        for (int i = 0; i < visitCount; i++) {
            visitsToStore[i] = getVisitContentValues(
                    guid, (JSONObject) visits.get(i), DEFAULT_IS_LOCAL_VALUE);
        }
        return visitsToStore;
    }

    /**
     * Maps up to <code>limit</code> visits for a given history GUID to an array of JSONObjects with "date" and "type" keys
     *
     * @param contentClient <code>ContentProviderClient</code> to use for querying Visits table
     * @param guid History GUID for which to return visits
     * @param limit Will return at most this number of visits
     * @return <code>JSONArray</code> of all visits found for given History GUID
     */
    public static JSONArray getRecentHistoryVisitsForGUID(@NonNull ContentProviderClient contentClient,
                                                          @NonNull String guid, int limit) throws RemoteException {
        final JSONArray visits = new JSONArray();

        final Cursor cursor = contentClient.query(
                visitsUriWithLimit(limit),
                new String[] {Visits.VISIT_TYPE, Visits.DATE_VISITED},
                Visits.HISTORY_GUID + " = ?",
                new String[] {guid}, null);
        if (cursor == null) {
            return visits;
        }
        try {
            if (!cursor.moveToFirst()) {
                return visits;
            }

            final int dateVisitedCol = cursor.getColumnIndexOrThrow(Visits.DATE_VISITED);
            final int visitTypeCol = cursor.getColumnIndexOrThrow(Visits.VISIT_TYPE);

            while (!cursor.isAfterLast()) {
                insertTupleIntoVisitsUnchecked(visits,
                        cursor.getLong(visitTypeCol),
                        cursor.getLong(dateVisitedCol)
                );
                cursor.moveToNext();
            }
        } finally {
            cursor.close();
        }

        return visits;
    }

    /**
     * Constructs <code>ContentValues</code> object for a visit based on passed in parameters.
     *
     * @param visit <code>JSONObject</code> containing visit type and visit date keys for the visit
     * @param guid History GUID with with to associate this visit
     * @param isLocal Whether or not to mark this visit as local
     * @return <code>ContentValues</code> with all visit values necessary for database insertion
     * @throws IllegalArgumentException if visit object is missing date or type keys
     */
    public static ContentValues getVisitContentValues(@NonNull String guid, @NonNull JSONObject visit, boolean isLocal) {
        if (!visit.containsKey(SYNC_DATE_KEY) || !visit.containsKey(SYNC_TYPE_KEY)) {
            throw new IllegalArgumentException("Visit missing required keys");
        }

        final ContentValues cv = new ContentValues();
        cv.put(Visits.HISTORY_GUID, guid);
        cv.put(Visits.IS_LOCAL, isLocal ? Visits.VISIT_IS_LOCAL : Visits.VISIT_IS_REMOTE);
        cv.put(Visits.VISIT_TYPE, (Long) visit.get(SYNC_TYPE_KEY));
        cv.put(Visits.DATE_VISITED, (Long) visit.get(SYNC_DATE_KEY));

        return cv;
    }

    @SuppressWarnings("unchecked")
    private static void insertTupleIntoVisitsUnchecked(@NonNull final JSONArray visits, @NonNull Long type, @NonNull Long date) {
        final JSONObject visit = new JSONObject();
        visit.put(SYNC_TYPE_KEY, type);
        visit.put(SYNC_DATE_KEY, date);
        visits.add(visit);
    }

    private static Uri visitsUriWithLimit(int limit) {
        return BrowserContractHelpers.VISITS_CONTENT_URI
                .buildUpon()
                .appendQueryParameter("limit", Integer.toString(limit))
                .build();
    }
}
