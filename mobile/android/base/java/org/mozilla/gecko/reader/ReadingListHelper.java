/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reader;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.favicons.Favicons;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.util.UIAsyncTask;

import android.content.Context;
import android.util.Log;

public final class ReadingListHelper implements NativeEventListener {
    private static final String LOGTAG = "GeckoReadingListHelper";

    protected final Context context;
    private final BrowserDB db;

    public ReadingListHelper(Context context, GeckoProfile profile) {
        this.context = context;
        this.db = profile.getDB();

        EventDispatcher.getInstance().registerGeckoThreadListener((NativeEventListener) this,
            "Reader:FaviconRequest", "Reader:AddedToCache");
    }

    public void uninit() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener((NativeEventListener) this,
            "Reader:FaviconRequest", "Reader:AddedToCache");
    }

    @Override
    public void handleMessage(final String event, final NativeJSObject message,
                              final EventCallback callback) {
        switch (event) {
            case "Reader:FaviconRequest": {
                handleReaderModeFaviconRequest(callback, message.getString("url"));
                break;
            }
            case "Reader:AddedToCache": {
                // AddedToCache is a one way message: callback will be null, and we therefore shouldn't
                // attempt to handle it.
                handleAddedToCache(message.getString("url"), message.getString("path"), message.getInt("size"));
                break;
            }
        }
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

    private void handleAddedToCache(final String url, final String path, final int size) {
        final SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);

        rch.put(url, path, size);
    }

    public static void cacheReaderItem(final String url, final int tabID, Context context) {
        if (AboutPages.isAboutReader(url)) {
            throw new IllegalArgumentException("Page url must be original (not about:reader) url");
        }

        SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);

        if (!rch.isURLCached(url)) {
            GeckoAppShell.notifyObservers("Reader:AddToCache", Integer.toString(tabID));
        }
    }

    public static void removeCachedReaderItem(final String url, Context context) {
        if (AboutPages.isAboutReader(url)) {
            throw new IllegalArgumentException("Page url must be original (not about:reader) url");
        }

        SavedReaderViewHelper rch = SavedReaderViewHelper.getSavedReaderViewHelper(context);

        if (rch.isURLCached(url)) {
            GeckoAppShell.notifyObservers("Reader:RemoveFromCache", url);
        }

        // When removing items from the cache we can probably spare ourselves the async callback
        // that we use when adding cached items. We know the cached item will be gone, hence
        // we no longer need to track it in the SavedReaderViewHelper
        rch.remove(url);
    }
}
