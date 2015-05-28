/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.db;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.regex.Pattern;

import org.json.JSONArray;
import org.json.JSONException;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Log;

public class LocalTabsAccessor implements TabsAccessor {
    private static final String LOGTAG = "GeckoTabsAccessor";

    public static final String[] TABS_PROJECTION_COLUMNS = new String[] {
                                                                BrowserContract.Tabs.TITLE,
                                                                BrowserContract.Tabs.URL,
                                                                BrowserContract.Clients.GUID,
                                                                BrowserContract.Clients.NAME,
                                                                BrowserContract.Tabs.LAST_USED,
                                                                BrowserContract.Clients.LAST_MODIFIED,
                                                                BrowserContract.Clients.DEVICE_TYPE,
                                                            };

    public static final String[] CLIENTS_PROJECTION_COLUMNS = new String[] {
                                                                    BrowserContract.Clients.GUID,
                                                                    BrowserContract.Clients.NAME,
                                                                    BrowserContract.Clients.LAST_MODIFIED,
                                                                    BrowserContract.Clients.DEVICE_TYPE
                                                            };

    private static final String REMOTE_CLIENTS_SELECTION = BrowserContract.Clients.GUID + " IS NOT NULL";
    private static final String LOCAL_TABS_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS NULL";
    private static final String REMOTE_TABS_SELECTION = BrowserContract.Tabs.CLIENT_GUID + " IS NOT NULL";

    private static final String REMOTE_TABS_SORT_ORDER =
            // Most recently synced clients first.
            BrowserContract.Clients.LAST_MODIFIED + " DESC, " +
            // If two clients somehow had the same last modified time, this will
            // group them (arbitrarily).
            BrowserContract.Clients.GUID + " DESC, " +
            // Within a single client, most recently used tabs first.
            BrowserContract.Tabs.LAST_USED + " DESC";

    private static final String LOCAL_CLIENT_SELECTION = BrowserContract.Clients.GUID + " IS NULL";

    private static final Pattern FILTERED_URL_PATTERN = Pattern.compile("^(about|chrome|wyciwyg|file):");

    private final Uri clientsRecencyUriWithProfile;
    private final Uri tabsUriWithProfile;
    private final Uri clientsUriWithProfile;

    public LocalTabsAccessor(String profileName) {
        tabsUriWithProfile = DBUtils.appendProfileWithDefault(profileName, BrowserContract.Tabs.CONTENT_URI);
        clientsUriWithProfile = DBUtils.appendProfileWithDefault(profileName, BrowserContract.Clients.CONTENT_URI);
        clientsRecencyUriWithProfile = DBUtils.appendProfileWithDefault(profileName, BrowserContract.Clients.CONTENT_RECENCY_URI);
    }

    /**
     * Extracts a List of just RemoteClients from a cursor.
     * The supplied cursor should be grouped by guid and sorted by most recently used.
     */
    @Override
    public List<RemoteClient> getClientsWithoutTabsByRecencyFromCursor(Cursor cursor) {
        final ArrayList<RemoteClient> clients = new ArrayList<>(cursor.getCount());

        final int originalPosition = cursor.getPosition();
        try {
            if (!cursor.moveToFirst()) {
                return clients;
            }

            final int clientGuidIndex = cursor.getColumnIndex(BrowserContract.Clients.GUID);
            final int clientNameIndex = cursor.getColumnIndex(BrowserContract.Clients.NAME);
            final int clientLastModifiedIndex = cursor.getColumnIndex(BrowserContract.Clients.LAST_MODIFIED);
            final int clientDeviceTypeIndex = cursor.getColumnIndex(BrowserContract.Clients.DEVICE_TYPE);

            while (!cursor.isAfterLast()) {
                final String clientGuid = cursor.getString(clientGuidIndex);
                final String clientName = cursor.getString(clientNameIndex);
                final String deviceType = cursor.getString(clientDeviceTypeIndex);
                final long lastModified = cursor.getLong(clientLastModifiedIndex);

                clients.add(new RemoteClient(clientGuid, clientName, lastModified, deviceType));

                cursor.moveToNext();
            }
        } finally {
            cursor.moveToPosition(originalPosition);
        }
        return clients;
    }

    /**
     * Extract client and tab records from a cursor.
     * <p>
     * The position of the cursor is moved to before the first record before
     * reading. The cursor is advanced until there are no more records to be
     * read. The position of the cursor is restored before returning.
     *
     * @param cursor
     *            to extract records from. The records should already be grouped
     *            by client GUID.
     * @return list of clients, each containing list of tabs.
     */
    @Override
    public List<RemoteClient> getClientsFromCursor(final Cursor cursor) {
        final ArrayList<RemoteClient> clients = new ArrayList<RemoteClient>();

        final int originalPosition = cursor.getPosition();
        try {
            if (!cursor.moveToFirst()) {
                return clients;
            }

            final int tabTitleIndex = cursor.getColumnIndex(BrowserContract.Tabs.TITLE);
            final int tabUrlIndex = cursor.getColumnIndex(BrowserContract.Tabs.URL);
            final int tabLastUsedIndex = cursor.getColumnIndex(BrowserContract.Tabs.LAST_USED);
            final int clientGuidIndex = cursor.getColumnIndex(BrowserContract.Clients.GUID);
            final int clientNameIndex = cursor.getColumnIndex(BrowserContract.Clients.NAME);
            final int clientLastModifiedIndex = cursor.getColumnIndex(BrowserContract.Clients.LAST_MODIFIED);
            final int clientDeviceTypeIndex = cursor.getColumnIndex(BrowserContract.Clients.DEVICE_TYPE);

            // A walking partition, chunking by client GUID. We assume the
            // cursor records are already grouped by client GUID; see the query
            // sort order.
            RemoteClient lastClient = null;
            while (!cursor.isAfterLast()) {
                final String clientGuid = cursor.getString(clientGuidIndex);
                if (lastClient == null || !TextUtils.equals(lastClient.guid, clientGuid)) {
                    final String clientName = cursor.getString(clientNameIndex);
                    final long lastModified = cursor.getLong(clientLastModifiedIndex);
                    final String deviceType = cursor.getString(clientDeviceTypeIndex);
                    lastClient = new RemoteClient(clientGuid, clientName, lastModified, deviceType);
                    clients.add(lastClient);
                }

                final String tabTitle = cursor.getString(tabTitleIndex);
                final String tabUrl = cursor.getString(tabUrlIndex);
                final long tabLastUsed = cursor.getLong(tabLastUsedIndex);
                lastClient.tabs.add(new RemoteTab(tabTitle, tabUrl, tabLastUsed));

                cursor.moveToNext();
            }
        } finally {
            cursor.moveToPosition(originalPosition);
        }

        return clients;
    }

    @Override
    public Cursor getRemoteClientsByRecencyCursor(Context context) {
        final Uri uri = clientsRecencyUriWithProfile;
        return context.getContentResolver().query(uri, CLIENTS_PROJECTION_COLUMNS,
                REMOTE_CLIENTS_SELECTION, null, null);
    }

    @Override
    public Cursor getRemoteTabsCursor(Context context) {
        return getRemoteTabsCursor(context, -1);
    }

    @Override
    public Cursor getRemoteTabsCursor(Context context, int limit) {
        Uri uri = tabsUriWithProfile;

        if (limit > 0) {
            uri = uri.buildUpon()
                     .appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(limit))
                     .build();
        }

        final Cursor cursor =  context.getContentResolver().query(uri,
                                                            TABS_PROJECTION_COLUMNS,
                                                            REMOTE_TABS_SELECTION,
                                                            null,
                                                            REMOTE_TABS_SORT_ORDER);
        return cursor;
    }

    // This method returns all tabs from all remote clients,
    // ordered by most recent client first, most recent tab first
    @Override
    public void getTabs(final Context context, final OnQueryTabsCompleteListener listener) {
        getTabs(context, 0, listener);
    }

    // This method returns limited number of tabs from all remote clients,
    // ordered by most recent client first, most recent tab first
    @Override
    public void getTabs(final Context context, final int limit, final OnQueryTabsCompleteListener listener) {
        // If there is no listener, no point in doing work.
        if (listener == null)
            return;

        (new UIAsyncTask.WithoutParams<List<RemoteClient>>(ThreadUtils.getBackgroundHandler()) {
            @Override
            protected List<RemoteClient> doInBackground() {
                final Cursor cursor = getRemoteTabsCursor(context, limit);
                if (cursor == null)
                    return null;

                try {
                    return Collections.unmodifiableList(getClientsFromCursor(cursor));
                } finally {
                    cursor.close();
                }
            }

            @Override
            protected void onPostExecute(List<RemoteClient> clients) {
                listener.onQueryTabsComplete(clients);
            }
        }).execute();
    }

    // Updates the modified time of the local client with the current time.
    private void updateLocalClient(final ContentResolver cr) {
        ContentValues values = new ContentValues();
        values.put(BrowserContract.Clients.LAST_MODIFIED, System.currentTimeMillis());

        cr.update(clientsUriWithProfile, values, LOCAL_CLIENT_SELECTION, null);
    }

    // Deletes all local tabs.
    private void deleteLocalTabs(final ContentResolver cr) {
        cr.delete(tabsUriWithProfile, LOCAL_TABS_SELECTION, null);
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
    private void insertLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs) {
        // Reuse this for serializing individual history URLs as JSON.
        JSONArray history = new JSONArray();
        ArrayList<ContentValues> valuesToInsert = new ArrayList<ContentValues>();

        int position = 0;
        for (Tab tab : tabs) {
            // Skip this tab if it has a null URL or is in private browsing mode, or is a filtered URL.
            String url = tab.getURL();
            if (url == null || tab.isPrivate() || isFilteredURL(url))
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
        cr.bulkInsert(tabsUriWithProfile, valuesToInsertArray);
    }

    // Deletes all local tabs and replaces them with a new list of tabs.
    @Override
    public synchronized void persistLocalTabs(final ContentResolver cr, final Iterable<Tab> tabs) {
        deleteLocalTabs(cr);
        insertLocalTabs(cr, tabs);
        updateLocalClient(cr);
    }

    /**
     * Matches the supplied URL string against the set of URLs to filter.
     *
     * @return true if the supplied URL should be skipped; false otherwise.
     */
    private boolean isFilteredURL(String url) {
        return FILTERED_URL_PATTERN.matcher(url).lookingAt();
    }
}
