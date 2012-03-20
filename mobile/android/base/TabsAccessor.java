/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.mozilla.gecko.db.BrowserContract.Clients;
import org.mozilla.gecko.db.BrowserContract.Tabs;
import org.mozilla.gecko.db.BrowserContract;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.util.Log;

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

    public static class RemoteTab {
        public String title;
        public String url;
        public String guid;
        public String name;
    }

    public interface OnQueryTabsCompleteListener {
        public void onQueryTabsComplete(List<RemoteTab> tabs);
    }

    public interface OnClientsAvailableListener {
        public void areAvailable(boolean available);
    }

    // Helper method to check if there are any clients available
    public static void areClientsAvailable(final Context context, final OnClientsAvailableListener listener) {
        if (listener == null)
            return;

        (new GeckoAsyncTask<Void, Void, Boolean> () {
            @Override
            protected Boolean doInBackground(Void... unused) {
                Uri uri = BrowserContract.Tabs.CONTENT_URI;
                uri = uri.buildUpon()
                         .appendQueryParameter(BrowserContract.PARAM_LIMIT, "1")
                         .build();

                Cursor cursor = context.getContentResolver().query(uri,
                                                                   CLIENTS_AVAILABILITY_PROJECTION,
                                                                   CLIENTS_SELECTION,
                                                                   null,
                                                                   null);
                
                if (cursor == null)
                    return false;
                
                try {
                    return cursor.moveToNext();
                } finally {
                    cursor.close();
                }
            }

            @Override
            protected void onPostExecute(Boolean availability) {
                listener.areAvailable(availability);
            }
        }).setPriority(GeckoAsyncTask.PRIORITY_HIGH).execute();
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

        (new GeckoAsyncTask<Void, Void, List<RemoteTab>> () {
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
}
