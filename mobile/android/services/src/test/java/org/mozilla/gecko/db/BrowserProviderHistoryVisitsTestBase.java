/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.db;

import android.content.ContentProvider;
import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.net.Uri;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Before;
import org.mozilla.gecko.background.db.DelegatingTestContentProvider;
import org.mozilla.gecko.sync.repositories.android.BrowserContractHelpers;
import org.robolectric.shadows.ShadowContentResolver;

public class BrowserProviderHistoryVisitsTestBase {
    /* package-private */ ShadowContentResolver contentResolver;
    /* package-private */ ContentProviderClient historyClient;
    /* package-private */ ContentProviderClient visitsClient;
    /* package-private */ Uri historyTestUri;
    /* package-private */ Uri visitsTestUri;
    /* package-private */ ContentProvider provider;

    @Before
    public void setUp() throws Exception {
        provider = DelegatingTestContentProvider.createDelegatingBrowserProvider();

        contentResolver = new ShadowContentResolver();
        historyClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.HISTORY_CONTENT_URI);
        visitsClient = contentResolver.acquireContentProviderClient(BrowserContractHelpers.VISITS_CONTENT_URI);

        historyTestUri = testUri(BrowserContract.History.CONTENT_URI);
        visitsTestUri = testUri(BrowserContract.Visits.CONTENT_URI);
    }

    @After
    public void tearDown() {
        historyClient.release();
        visitsClient.release();
        provider.shutdown();
    }

    /* package-private */  Uri testUri(Uri baseUri) {
        return baseUri.buildUpon().appendQueryParameter(BrowserContract.PARAM_IS_TEST, "1").build();
    }

    /* package-private */  Uri insertHistoryItem(String url, String guid) throws RemoteException {
        return insertHistoryItem(url, guid, System.currentTimeMillis(), null, null, null);
    }

    /* package-private */  Uri insertHistoryItem(String url, String guid, Long lastVisited, Integer visitCount) throws RemoteException {
        return insertHistoryItem(url, guid, lastVisited, visitCount, null, null);
    }

    /* package-private */  Uri insertHistoryItem(String url, String guid, Long lastVisited, Integer visitCount, String title) throws RemoteException {
        return insertHistoryItem(url, guid, lastVisited, visitCount, null, title);
    }

    /* package-private */  Uri insertHistoryItem(String url, String guid, Long lastVisited, Integer visitCount, Integer remoteVisits, String title) throws RemoteException {
        ContentValues historyItem = new ContentValues();
        historyItem.put(BrowserContract.History.URL, url);
        if (guid != null) {
            historyItem.put(BrowserContract.History.GUID, guid);
        }
        if (visitCount != null) {
            historyItem.put(BrowserContract.History.VISITS, visitCount);
        }
        if (remoteVisits != null) {
            historyItem.put(BrowserContract.History.REMOTE_VISITS, remoteVisits);
        }
        historyItem.put(BrowserContract.History.DATE_LAST_VISITED, lastVisited);
        if (title != null) {
            historyItem.put(BrowserContract.History.TITLE, title);
        }

        return historyClient.insert(historyTestUri, historyItem);
    }
}
