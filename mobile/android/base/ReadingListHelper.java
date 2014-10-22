/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import org.json.JSONException;
import org.json.JSONObject;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.util.Log;
import android.widget.Toast;

public final class ReadingListHelper implements GeckoEventListener, NativeEventListener {
    private static final String LOGTAG = "ReadingListHelper";

    private static final int READER_ADD_SUCCESS = 0;
    private static final int READER_ADD_FAILED = 1;
    private static final int READER_ADD_DUPLICATE = 2;

    protected final Context context;


    public ReadingListHelper(Context context) {
        this.context = context;

        EventDispatcher.getInstance().registerGeckoThreadListener((GeckoEventListener) this,
            "Reader:AddToList", "Reader:FaviconRequest");
        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener) this,
            "Reader:ListStatusRequest", "Reader:Removed");
    }

    public void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener((GeckoEventListener) this,
            "Reader:AddToList", "Reader:FaviconRequest");
        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener) this,
            "Reader:ListStatusRequest", "Reader:Removed");
    }

    @Override
    public void handleMessage(String event, JSONObject message) {
        switch(event) {
            case "Reader:AddToList": {
                handleAddToList(message);
                break;
            }

            case "Reader:FaviconRequest": {
                handleReaderModeFaviconRequest(message.optString("url"));
                break;
            }
        }
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        switch(event) {
            case "Reader:Removed": {
                handleReadingListRemoved(message.getString("url"));
                break;
            }

            case "Reader:ListStatusRequest": {
                handleReadingListStatusRequest(message.getString("url"));
                break;
            }
        }
    }

    /**
     * A page can be added to the ReadingList by long-tap of the page-action
     * icon, or by tapping the readinglist-add icon in the ReaderMode banner.
     */
    private void handleAddToList(JSONObject message) {
        final int result = message.optInt("result", READER_ADD_FAILED);
        if (result != READER_ADD_SUCCESS) {
            if (result == READER_ADD_FAILED) {
                showToast(R.string.reading_list_failed, Toast.LENGTH_SHORT);
            } else if (result == READER_ADD_DUPLICATE) {
                showToast(R.string.reading_list_duplicate, Toast.LENGTH_SHORT);
            }
            return;
        }

        final ContentValues values = new ContentValues();
        final String url = message.optString("url");

        values.put(ReadingListItems.URL, url);
        values.put(ReadingListItems.TITLE, message.optString("title"));
        values.put(ReadingListItems.LENGTH, message.optInt("length"));
        values.put(ReadingListItems.EXCERPT, message.optString("excerpt"));

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.addReadingListItem(context.getContentResolver(), values);
                showToast(R.string.reading_list_added, Toast.LENGTH_SHORT);
            }
        });

        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Reader:Added", url));
    }

    /**
     * Gecko (ReaderMode) requests the page favicon to append to the
     * document head for display.
     */
    private void handleReaderModeFaviconRequest(final String url) {
        (new UIAsyncTask.WithoutParams<String>(ThreadUtils.getBackgroundHandler()) {
            @Override
            public String doInBackground() {
                return Favicons.getFaviconURLForPageURL(context, url);
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

                GeckoAppShell.sendEventToGecko(
                    GeckoEvent.createBroadcastEvent("Reader:FaviconReturn", args.toString()));
            }
        }).execute();
    }

    /**
     * A page can be removed from the ReadingList by panel context menu,
     * or by tapping the readinglist-remove icon in the ReaderMode banner.
     */
    private void handleReadingListRemoved(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                BrowserDB.removeReadingListItemWithURL(context.getContentResolver(), url);
                showToast(R.string.page_removed, Toast.LENGTH_SHORT);
            }
        });
    }

    /**
     * Gecko (ReaderMode) requests the page ReadingList status, to display
     * the proper ReaderMode banner icon (readinglist-add / readinglist-remove).
     */
    private void handleReadingListStatusRequest(final String url) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                final int inReadingList =
                    BrowserDB.isReadingListItem(context.getContentResolver(), url) ? 1 : 0;

                final JSONObject json = new JSONObject();
                try {
                    json.put("url", url);
                    json.put("inReadingList", inReadingList);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "JSON error - failed to return inReadingList status", e);
                    return;
                }

                GeckoAppShell.sendEventToGecko(
                    GeckoEvent.createBroadcastEvent("Reader:ListStatusReturn", json.toString()));
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
}
