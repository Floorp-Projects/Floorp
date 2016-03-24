/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reader;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.annotation.RobocopTarget;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.ReadingListAccessor;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.util.Log;

public final class ReadingListHelper implements NativeEventListener {
    private static final String LOGTAG = "GeckoReadingListHelper";

    public interface OnReadingListEventListener {
        void onAddedToReadingList(String url);
        void onRemovedFromReadingList(String url);
        void onAlreadyInReadingList(String url);
    }

    private enum ReadingListEvent {
        ADDED,
        REMOVED,
        ALREADY_EXISTS
    }

    protected final Context context;
    private final BrowserDB db;
    private final ReadingListAccessor readingListAccessor;
    private final ContentObserver contentObserver;
    private final OnReadingListEventListener onReadingListEventListener;

    volatile boolean fetchInBackground = true;

    public ReadingListHelper(Context context, GeckoProfile profile, OnReadingListEventListener listener) {
        this.context = context;
        this.db = profile.getDB();
        this.readingListAccessor = db.getReadingListAccessor();

        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener) this,
            "Reader:AddToList", "Reader:UpdateList", "Reader:FaviconRequest");


        contentObserver = new ContentObserver(null) {
            @Override
            public void onChange(boolean selfChange) {
                if (fetchInBackground) {
                    fetchContent();
                }
            }
        };

        this.readingListAccessor.registerContentObserver(context, contentObserver);

        onReadingListEventListener = listener;
    }

    public void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener) this,
            "Reader:AddToList", "Reader:UpdateList", "Reader:FaviconRequest");

        context.getContentResolver().unregisterContentObserver(contentObserver);
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        switch(event) {
            // Added from web context menu.
            case "Reader:AddToList": {
                handleAddToList(callback, message);
                break;
            }
            case "Reader:UpdateList": {
                handleUpdateList(message);
                break;
            }
            case "Reader:FaviconRequest": {
                handleReaderModeFaviconRequest(callback, message.getString("url"));
                break;
            }
        }
    }

    /**
     * A page can be added to the ReadingList by long-tap of the page-action
     * icon, or by tapping the readinglist-add icon in the ReaderMode banner.
     *
     * This method will only add new items, not update existing items.
     */
    private void handleAddToList(final EventCallback callback, final NativeJSObject message) {
        final ContentResolver cr = context.getContentResolver();
        final String url = message.getString("url");

        // We can't access a NativeJSObject from the background thread, so we need to get the
        // values here, even if we may not use them to insert an item into the DB.
        final ContentValues values = getContentValues(message);

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                if (readingListAccessor.isReadingListItem(cr, url)) {
                    handleEvent(ReadingListEvent.ALREADY_EXISTS, url);
                    callback.sendError("URL already in reading list: " + url);
                } else {
                    readingListAccessor.addReadingListItem(cr, values);
                    handleEvent(ReadingListEvent.ADDED, url);
                    callback.sendSuccess(url);
                }
            }
        });
    }

    /**
     * Updates a reading list item with new meta data.
     */
    private void handleUpdateList(final NativeJSObject message) {
        final ContentResolver cr = context.getContentResolver();
        final ContentValues values = getContentValues(message);

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                readingListAccessor.updateReadingListItem(cr, values);
            }
        });
    }

    /**
     * Creates reading list item content values from JS message.
     */
    private ContentValues getContentValues(NativeJSObject message) {
        final ContentValues values = new ContentValues();
        if (message.has("id")) {
            values.put(ReadingListItems._ID, message.getInt("id"));
        }

        // url is actually required...
        String url = null;
        if (message.has("url")) {
            url = message.getString("url");
            values.put(ReadingListItems.URL, url);
        }

        String title = null;
        if (message.has("title")) {
            title = message.getString("title");
            values.put(ReadingListItems.TITLE, title);
        }

        // TODO: message actually has "length", but that's no use for us. See Bug 1127451.
        if (message.has("word_count")) {
            values.put(ReadingListItems.WORD_COUNT, message.getInt("word_count"));
        }

        if (message.has("excerpt")) {
            values.put(ReadingListItems.EXCERPT, message.getString("excerpt"));
        }

        if (message.has("status")) {
            final int status = message.getInt("status");
            values.put(ReadingListItems.CONTENT_STATUS, status);
            if (status == ReadingListItems.STATUS_FETCHED_ARTICLE) {
                if (message.has("resolved_title")) {
                    values.put(ReadingListItems.RESOLVED_TITLE, message.getString("resolved_title"));
                } else {
                    if (title != null) {
                        values.put(ReadingListItems.RESOLVED_TITLE, title);
                    }
                }
                if (message.has("resolved_url")) {
                    values.put(ReadingListItems.RESOLVED_URL, message.getString("resolved_url"));
                } else {
                    if (url != null) {
                        values.put(ReadingListItems.RESOLVED_URL, url);
                    }
                }
            }
        }

        return values;
    }

    /**
     * Gecko (ReaderMode) requests the page favicon to append to the
     * document head for display.
     */
    private void handleReaderModeFaviconRequest(final EventCallback callback, final String url) {
        (new UIAsyncTask.WithoutParams<String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public String doInBackground() {
                return Favicons.getFaviconURLForPageURL(db, context.getContentResolver(), url);
            }

            @Override
            public void onPostExecute(String faviconUrl) {
                JSONObject args = new JSONObject();
                if (faviconUrl != null) {
                    try {
                        args.put("url", url);
                        args.put("faviconUrl", faviconUrl);
                    } catch (JSONException e) {
                        Log.w(LOGTAG, "Error building JSON favicon arguments.", e);
                    }
                }
                callback.sendSuccess(args.toString());
            }
        }).execute();
    }

    /**
     * Handle various reading list events (and display appropriate toasts).
     */
    private void handleEvent(final ReadingListEvent event, final String url) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                switch(event) {
                    case ADDED:
                        onReadingListEventListener.onAddedToReadingList(url);
                        break;
                    case REMOVED:
                        onReadingListEventListener.onRemovedFromReadingList(url);
                        break;
                    case ALREADY_EXISTS:
                        onReadingListEventListener.onAlreadyInReadingList(url);
                        break;
                }
            }
        });
    }

    private void fetchContent() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final Cursor c = readingListAccessor.getReadingListUnfetched(context.getContentResolver());
                if (c == null) {
                    return;
                }
                try {
                    while (c.moveToNext()) {
                        JSONObject json = new JSONObject();
                        try {
                            json.put("id", c.getInt(c.getColumnIndexOrThrow(ReadingListItems._ID)));
                            json.put("url", c.getString(c.getColumnIndexOrThrow(ReadingListItems.URL)));
                            GeckoAppShell.notifyObservers("Reader:FetchContent", json.toString());
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Failed to fetch reading list content for item");
                        }
                    }
                } finally {
                    c.close();
                }
            }
        });
    }

    @RobocopTarget
    /**
     * Test code will want to disable background fetches to avoid upsetting
     * the test harness. Call this by accessing the instance from BrowserApp.
     */
    public void disableBackgroundFetches() {
        fetchInBackground = false;
    }
}
