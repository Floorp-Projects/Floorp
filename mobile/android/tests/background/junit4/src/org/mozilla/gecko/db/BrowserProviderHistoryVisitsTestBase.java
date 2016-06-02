/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.net.Uri;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Before;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.shadows.ShadowContentResolver;

public class BrowserProviderHistoryVisitsTestBase {
    protected BrowserProvider provider;
    protected ContentProviderClient historyClient;
    protected ContentProviderClient visitsClient;
    protected Uri historyTestUri;
    protected Uri visitsTestUri;

    @Before
    public void setUp() throws Exception {
        provider = new BrowserProvider();
        provider.onCreate();
        ShadowContentResolver.registerProvider(BrowserContract.AUTHORITY_URI.toString(), provider);

        final ShadowContentResolver cr = new ShadowContentResolver();
        historyClient = cr.acquireContentProviderClient(BrowserContractHelpers.HISTORY_CONTENT_URI);
        visitsClient = cr.acquireContentProviderClient(BrowserContractHelpers.VISITS_CONTENT_URI);

        historyTestUri = testUri(BrowserContract.History.CONTENT_URI);
        visitsTestUri = testUri(BrowserContract.Visits.CONTENT_URI);
    }

    @After
    public void tearDown() {
        historyClient.release();
        visitsClient.release();
        provider.shutdown();
    }

    protected Uri testUri(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }

    protected Uri insertHistoryItem(String url, String guid) throws RemoteException {
        return insertHistoryItem(url, guid, System.currentTimeMillis(), null);
    }

    protected Uri insertHistoryItem(String url, String guid, Long lastVisited, Integer visitCount) throws RemoteException {
        ContentValues historyItem = new ContentValues();
        historyItem.put(BrowserContract.History.URL, url);
        if (guid != null) {
            historyItem.put(BrowserContract.History.GUID, guid);
        }
        if (visitCount != null) {
            historyItem.put(BrowserContract.History.VISITS, visitCount);
        }
        historyItem.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);

        return historyClient.insert(historyTestUri, historyItem);
    }
}
