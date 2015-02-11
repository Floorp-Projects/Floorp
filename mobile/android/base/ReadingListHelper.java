/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.DBUtils;
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
import android.net.Uri;
import android.util.Log;
import android.widget.Toast;

public final class ReadingListHelper implements NativeEventListener {
    private static final String LOGTAG = "GeckoReadingListHelper";

    protected final Context context;
    private final BrowserDB db;
    private final ReadingListAccessor readingListAccessor;
    private final ContentObserver contentObserver;

    public ReadingListHelper(Context context, GeckoProfile profile) {
        this.context = context;
        this.db = profile.getDB();
        this.readingListAccessor = db.getReadingListAccessor();

        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener) this,
            "Reader:AddToList", "Reader:UpdateList", "Reader:FaviconRequest", "Reader:ListStatusRequest", "Reader:RemoveFromList");


        contentObserver = new ContentObserver(null) {
            @Override
            public void onChange(boolean selfChange) {
                fetchContent();
            }
        };

        this.readingListAccessor.registerContentObserver(context, contentObserver);
    }

    public void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener) this,
            "Reader:AddToList", "Reader:UpdateList", "Reader:FaviconRequest", "Reader:ListStatusRequest", "Reader:RemoveFromList");

        context.getContentResolver().unregisterContentObserver(contentObserver);
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        switch(event) {
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
            case "Reader:RemoveFromList": {
                handleRemoveFromList(message.getString("url"));
                break;
            }
            case "Reader:ListStatusRequest": {
                handleReadingListStatusRequest(callback, message.getString("url"));
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
                    showToast(R.string.reading_list_duplicate, Toast.LENGTH_SHORT);
                    callback.sendError("URL already in reading list: " + url);
                } else {
                    readingListAccessor.addReadingListItem(cr, values);
                    showToast(R.string.reading_list_added, Toast.LENGTH_SHORT);
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
        if (message.has("url")) {
            values.put(ReadingListItems.URL, message.getString("url"));
        }
        if (message.has("title")) {
            values.put(ReadingListItems.TITLE, message.getString("title"));
        }
        if (message.has("length")) {
            values.put(ReadingListItems.LENGTH, message.getInt("length"));
        }
        if (message.has("excerpt")) {
            values.put(ReadingListItems.EXCERPT, message.getString("excerpt"));
        }
        if (message.has("status")) {
            values.put(ReadingListItems.CONTENT_STATUS, message.getInt("status"));
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
     * A page can be removed from the ReadingList by panel context menu,
     * or by tapping the readinglist-remove icon in the ReaderMode banner.
     */
    private void handleRemoveFromList(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                readingListAccessor.removeReadingListItemWithURL(context.getContentResolver(), url);
                GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:Removed", url));
                showToast(R.string.page_removed, Toast.LENGTH_SHORT);
            }
        });
    }

    /**
     * Gecko (ReaderMode) requests the page ReadingList status, to display
     * the proper ReaderMode banner icon (readinglist-add / readinglist-remove).
     */
    private void handleReadingListStatusRequest(final EventCallback callback, final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final int inReadingList = readingListAccessor.isReadingListItem(context.getContentResolver(), url) ? 1 : 0;

                final JSONObject json = new JSONObject();
                try {
                    json.put("url", url);
                    json.put("inReadingList", inReadingList);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON error - failed to return inReadingList status", e);
                }

                // Return the json object to fulfill the promise.
                callback.sendSuccess(json.toString());
            }
        });
    }

    /**
     * Show various status toasts.
     */
    private void showToast(final int resId, final int duration) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(context, resId, duration).show();
            }
        });
    }

    private void fetchContent() {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final Cursor c = readingListAccessor.getReadingListUnfetched(context.getContentResolver());
                try {
                    while (c.moveToNext()) {
                        JSONObject json = new JSONObject();
                        try {
                            json.put("id", c.getInt(c.getColumnIndexOrThrow(ReadingListItems._ID)));
                            json.put("url", c.getString(c.getColumnIndexOrThrow(ReadingListItems.URL)));
                            GeckoAppShell.sendEventToGecko(
                                GeckoEvent.createBroadcastEvent("Reader:FetchContent", json.toString()));
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
}
