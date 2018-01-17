/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.android;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.net.Uri;

import junit.framework.Assert;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserProvider;
import org.robolectric.shadows.ShadowContentResolver;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

@RunWith(TestRunner.class)
public class VisitsHelperTest {
    @Test
    public void testBulkInsertRemoteVisits() throws Exception {
        JSONArray toInsert = new JSONArray();
        Assert.assertEquals(0, VisitsHelper.getVisitsContentValues("testGUID", toInsert).length);

        JSONObject visit = new JSONObject();
        Long date = Long.valueOf(123432552344l);
        visit.put("date", date);
        visit.put("type", 2l);
        toInsert.add(visit);

        JSONObject visit2 = new JSONObject();
        visit2.put("date", date + 1000);
        visit2.put("type", 5l);
        toInsert.add(visit2);

        ContentValues[] cvs = VisitsHelper.getVisitsContentValues("testGUID", toInsert);
        Assert.assertEquals(2, cvs.length);
        ContentValues cv1 = cvs[0];
        ContentValues cv2 = cvs[1];
        Assert.assertEquals(Integer.valueOf(2), cv1.getAsInteger(BrowserContract.Visits.VISIT_TYPE));
        Assert.assertEquals(Integer.valueOf(5), cv2.getAsInteger(BrowserContract.Visits.VISIT_TYPE));

        Assert.assertEquals(date, cv1.getAsLong("date"));
        Assert.assertEquals(Long.valueOf(date + 1000), cv2.getAsLong(BrowserContract.Visits.DATE_VISITED));
    }

    @Test
    public void testGetRecentHistoryVisitsForGUID() throws Exception {
        Uri historyTestUri = testUri(BrowserContract.History.CONTENT_URI);
        Uri visitsTestUri = testUri(BrowserContract.Visits.CONTENT_URI);

        BrowserProvider provider = new BrowserProvider();
        try {
            provider.onCreate();
            ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY, new DelegatingTestContentProvider(provider));

            final ShadowContentResolver cr = new ShadowContentResolver();
            ContentProviderClient historyClient = cr.acquireContentProviderClient(BrowserContractHelpers.HISTORY_CONTENT_URI);
            ContentProviderClient visitsClient = cr.acquireContentProviderClient(BrowserContractHelpers.VISITS_CONTENT_URI);

            ContentValues historyItem = new ContentValues();
            historyItem.put(BrowserContract.History.URL, "https://www.mozilla.org");
            historyItem.put(BrowserContract.History.GUID, "testGUID");
            historyClient.insert(historyTestUri, historyItem);

            Long baseDate = System.currentTimeMillis();
            for (int i = 0; i < 30; i++) {
                ContentValues visitItem = new ContentValues();
                visitItem.put(BrowserContract.Visits.HISTORY_GUID, "testGUID");
                visitItem.put(BrowserContract.Visits.DATE_VISITED, baseDate - i * 100);
                visitItem.put(BrowserContract.Visits.VISIT_TYPE, 1);
                visitItem.put(BrowserContract.Visits.IS_LOCAL, 1);
                visitsClient.insert(visitsTestUri, visitItem);
            }

            // test that limit worked, that sorting is correct, and that both date and type are present
            JSONArray recentVisits = VisitsHelper.getRecentHistoryVisitsForGUID(visitsClient, "testGUID", 10);
            Assert.assertEquals(10, recentVisits.size());
            for (int i = 0; i < recentVisits.size(); i++) {
                JSONObject v = (JSONObject) recentVisits.get(i);
                Long date = (Long) v.get("date");
                Long type = (Long) v.get("type");
                Assert.assertEquals(Long.valueOf(baseDate - i * 100), date);
                Assert.assertEquals(Long.valueOf(1), type);
            }
        } finally {
            provider.shutdown();
        }
    }

    @Test
    public void testGetVisitContentValues() throws Exception {
        JSONObject visit = new JSONObject();
        Long date = Long.valueOf(123432552344l);
        visit.put("date", date);
        visit.put("type", Long.valueOf(2));

        ContentValues cv = VisitsHelper.getVisitContentValues("testGUID", visit, true);
        assertTrue(cv.containsKey(BrowserContract.Visits.VISIT_TYPE));
        assertTrue(cv.containsKey(BrowserContract.Visits.DATE_VISITED));
        assertTrue(cv.containsKey(BrowserContract.Visits.HISTORY_GUID));
        assertTrue(cv.containsKey(BrowserContract.Visits.IS_LOCAL));
        assertEquals(4, cv.size());

        assertEquals(date, cv.getAsLong(BrowserContract.Visits.DATE_VISITED));
        assertEquals(Long.valueOf(2), cv.getAsLong(BrowserContract.Visits.VISIT_TYPE));
        assertEquals("testGUID", cv.getAsString(BrowserContract.Visits.HISTORY_GUID));
        assertEquals(Integer.valueOf(1), cv.getAsInteger(BrowserContract.Visits.IS_LOCAL));

        cv = VisitsHelper.getVisitContentValues("testGUID", visit, false);
        assertEquals(Integer.valueOf(0), cv.getAsInteger(BrowserContract.Visits.IS_LOCAL));

        try {
            JSONObject visit2 = new JSONObject();
            visit.put("date", date);
            VisitsHelper.getVisitContentValues("testGUID", visit2, false);
            assertTrue("Must check that visit type key is present", false);
        } catch (IllegalArgumentException e) {}

        try {
            JSONObject visit3 = new JSONObject();
            visit.put("type", Long.valueOf(2));
            VisitsHelper.getVisitContentValues("testGUID", visit3, false);
            assertTrue("Must check that visit date key is present", false);
        } catch (IllegalArgumentException e) {}

        try {
            JSONObject visit4 = new JSONObject();
            VisitsHelper.getVisitContentValues("testGUID", visit4, false);
            assertTrue("Must check that visit type and date keys are present", false);
        } catch (IllegalArgumentException e) {}
    }

    private Uri testUri(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }
}