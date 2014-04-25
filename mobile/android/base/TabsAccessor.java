/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UiAsyncTask;

import org.json.JSONArray;
import org.json.JSONException;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class TabsAccessor {
    private static final String LOGTAG = "GeckoTabsAccessor";

    private static final String[] CLIENTS_AVAILABILITY_PROJECTION = new String[] {
                                                                        BrowserContract.Clients.GUID
                                                                    };

    private static final String[] TABS_PROJECTION_COLUMNS = new String[] {
                                                                BrowserContract.Tabs.TITLE,
                                                                BrowserContract.Tabs.URL,
                                                                BrowserContract.Clients.GUID,
                                                                BrowserContract.Clients.NAME
                                                            };

    // Projection column numbers
    public static enum TABS_COLUMN {
        TITLE,
        URL,
        GUID,
        NAME
    };

    private static final String CLIENTS_SELECTION = BrowserContract.Clients.GUID + " IS NOT NULL";
    private static final String TABS_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS NOT NULL";

    private static final String LOCAL_CLIENT_SELECTION = BrowserContract.Clients.GUID + " IS NULL";
    private static final String LOCAL_TABS_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS NULL";

    public static class RemoteTab {
        public String title;
        public String url;
        public String guid;
        public String name;
    }

    public interface OnQueryTabsCompleteListener {
        public void onQueryTabsComplete(List<RemoteTab> tabs);
    }

    // This method returns all tabs from all remote clients, 
    // ordered by most recent client first, most recent tab first 
    public static void getTabs(final Context context, final OnQueryTabsCompleteListener listener) {
        getTabs(context, 0, listener);
    }

    // This method returns limited number of tabs from all remote clients, 
    // ordered by most recent client first, most recent tab first 
    public static void getTabs(final Context context, final int limit, final OnQueryTabsCompleteListener listener) {
        // If there is no listener, no point in doing work.
        if (listener == null)
            return;

        (new UiAsyncTask<Void, Void, List<RemoteTab>>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected List<RemoteTab> doInBackground(Void... unused) {
                Uri uri = BrowserContract.Tabs.CONTENT_URI;

                if (limit > 0) {
                    uri = uri.buildUpon()
                             .appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(limit))
                             .build();
                }
                    
                Cursor cursor =  context.getContentResolver().query(uri,
                                                                    TABS_PROJECTION_COLUMNS,
                                                                    TABS_SELECTION,
                                                                    null,
                                                                    null);
                
                if (cursor == null)
                    return null;
                
                RemoteTab tab;
                final ArrayList<RemoteTab> tabs = new ArrayList<RemoteTab> ();
                try {
                    while (cursor.moveToNext()) {
                        tab = new RemoteTab();
                        tab.title = cursor.getString(TABS_COLUMN.TITLE.ordinal());
                        tab.url = cursor.getString(TABS_COLUMN.URL.ordinal());
                        tab.guid = cursor.getString(TABS_COLUMN.GUID.ordinal());
                        tab.name = cursor.getString(TABS_COLUMN.NAME.ordinal());
                
                        tabs.add(tab);
                    }
                } finally {
                    cursor.close();
                }

                return Collections.unmodifiableList(tabs);
           }

            @Override
            protected void onPostExecute(List<RemoteTab> tabs) {
                listener.onQueryTabsComplete(tabs);
            }
        }).execute();
    }

    // Updates the modified time of the local client with the current time.
    private static void updateLocalClient(final ContentResolver cr) {
        ContentValues values = new ContentValues();
        values.put(BrowserContract.Clients.LAST_MODIFIED, System.currentTimeMillis());
        cr.update(BrowserContract.Clients.CONTENT_URI, values, LOCAL_CLIENT_SELECTION, null);
    }

    // Deletes all local tabs.
    private static void deleteLocalTabs(final ContentResolver cr) {
        cr.delete(BrowserContract.Tabs.CONTENT_URI, LOCAL_TABS_SELECTION, null);
    }

    /**
     * Tabs are positioned in the DB in the same order that they appear in the tabs param.
     *   - URL should never empty or null. Skip this tab if there's no URL.
     *   - TITLE should always a string, either a page title or empty.
     *   - LAST_USED should always be numeric.
     *   - FAVICON should be a URL or null.
     *   - HISTORY should be serialized JSON array of URLs.
     *   - POSITION should always be numeric.
     *   - CLIENT_GUID should always be null to represent the local client.
     */
    private static void insertLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs) {
        // Reuse this for serializing individual history URLs as JSON.
        JSONArray history = new JSONArray();
        ArrayList<ContentValues> valuesToInsert = new ArrayList<ContentValues>();

        int position = 0;
        for (Tab tab : tabs) {
            // Skip this tab if it has a null URL or is in private browsing mode
            String url = tab.getURL();
            if (url == null || tab.isPrivate())
                continue;

            ContentValues values = new ContentValues();
            values.put(BrowserContract.Tabs.URL, url);
            values.put(BrowserContract.Tabs.TITLE, tab.getTitle());
            values.put(BrowserContract.Tabs.LAST_USED, tab.getLastUsed());

            String favicon = tab.getFaviconURL();
            if (favicon != null)
                values.put(BrowserContract.Tabs.FAVICON, favicon);
            else
                values.putNull(BrowserContract.Tabs.FAVICON);

            // We don't have access to session history in Java, so for now, we'll
            // just use a JSONArray that holds most recent history item.
            try {
                history.put(0, tab.getURL());
                values.put(BrowserContract.Tabs.HISTORY, history.toString());
            } catch (JSONException e) {
                Log.w(LOGTAG, "JSONException adding URL to tab history array.", e);
            }

            values.put(BrowserContract.Tabs.POSITION, position++);

            // A null client guid corresponds to the local client.
            values.putNull(BrowserContract.Tabs.CLIENT_GUID);

            valuesToInsert.add(values);
        }

        ContentValues[] valuesToInsertArray = valuesToInsert.toArray(new ContentValues[valuesToInsert.size()]);
        cr.bulkInsert(BrowserContract.Tabs.CONTENT_URI, valuesToInsertArray);
    }

    // Deletes all local tabs and replaces them with a new list of tabs.
    public static synchronized void persistLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs) {
        deleteLocalTabs(cr);
        insertLocalTabs(cr, tabs);
        updateLocalClient(cr);
    }
}
