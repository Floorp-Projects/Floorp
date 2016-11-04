/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentProviderClient;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.ThreadUtils;

import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Provides access to metadata information about websites.
 *
 * While storing, in case of timing issues preventing us from looking up History GUID by a given uri,
 * we queue up metadata and wait for GlobalHistory to let us know history record is now available.
 *
 * TODO Bug 1313515: selection of metadata for a given uri/history_GUID
 *
 * @author grisha
 */
/* package-local */ class GlobalPageMetadata implements BundleEventListener {
    private static final String LOG_TAG = "GeckoGlobalPageMetadata";

    private static final GlobalPageMetadata instance = new GlobalPageMetadata();

    private static final String KEY_HAS_IMAGE = "hasImage";
    private static final String KEY_METADATA_JSON = "metadataJSON";

    private static final int MAX_METADATA_QUEUE_SIZE = 15;

    private final Map<String, Bundle> queuedMetadata = Collections.synchronizedMap(new LimitedLinkedHashMap<String, Bundle>());

    public static GlobalPageMetadata getInstance() {
        return instance;
    }

    private static class LimitedLinkedHashMap<K, V> extends LinkedHashMap<K, V> {
        private static final long serialVersionUID = 6359725112736360244L;

        @Override
        protected boolean removeEldestEntry(Entry<K, V> eldest) {
            if (size() > MAX_METADATA_QUEUE_SIZE) {
                Log.w(LOG_TAG, "Page metadata queue is full. Dropping oldest metadata.");
                return true;
            }
            return false;
        }
    }

    private GlobalPageMetadata() {}

    public void init() {
        EventDispatcher
                .getInstance()
                .registerBackgroundThreadListener(this, GlobalHistory.EVENT_URI_AVAILABLE_IN_HISTORY);
    }

    public void add(BrowserDB db, ContentProviderClient contentProviderClient, String uri, boolean hasImage, @NonNull String metadataJSON) {
        ThreadUtils.assertOnBackgroundThread();

        // NB: Other than checking that JSON is valid and trimming it,
        // we do not process metadataJSON in any way, trusting our source.
        doAddOrQueue(db, contentProviderClient, uri, hasImage, metadataJSON);
    }

    @VisibleForTesting
    /*package-local */ void doAddOrQueue(BrowserDB db, ContentProviderClient contentProviderClient, String uri, boolean hasImage, @NonNull String metadataJSON) {
        final String preparedMetadataJSON;
        try {
            preparedMetadataJSON = prepareJSON(metadataJSON);
        } catch (JSONException e) {
            Log.e(LOG_TAG, "Couldn't process metadata JSON", e);
            return;
        }

        // Don't bother queuing this if deletions fails to find a corresponding history record.
        // If we can't delete metadata because it didn't exist yet, that's OK.
        if (preparedMetadataJSON.equals("{}")) {
            final int deleted = db.deletePageMetadata(contentProviderClient, uri);
            // We could delete none if history record for uri isn't present.
            // We must delete one if history record for uri is present.
            if (deleted != 0 && deleted != 1) {
                throw new IllegalStateException("Deleted unexpected number of page metadata records: " + deleted);
            }
            return;
        }

        // If we could insert page metadata, we're done.
        if (db.insertPageMetadata(contentProviderClient, uri, hasImage, preparedMetadataJSON)) {
            return;
        }

        // Otherwise, we need to queue it for future insertion when history record is available.
        Bundle bundledMetadata = new Bundle();
        bundledMetadata.putBoolean(KEY_HAS_IMAGE, hasImage);
        bundledMetadata.putString(KEY_METADATA_JSON, preparedMetadataJSON);
        queuedMetadata.put(uri, bundledMetadata);
    }

    @VisibleForTesting
    /* package-local */ int getMetadataQueueSize() {
        return queuedMetadata.size();
    }

    @Override
    public void handleMessage(String event, Bundle message, EventCallback callback) {
        ThreadUtils.assertOnBackgroundThread();

        if (!GlobalHistory.EVENT_URI_AVAILABLE_IN_HISTORY.equals(event)) {
            return;
        }

        final String uri = message.getString(GlobalHistory.EVENT_PARAM_URI);
        if (TextUtils.isEmpty(uri)) {
            return;
        }

        final Bundle bundledMetadata;
        synchronized (queuedMetadata) {
            if (!queuedMetadata.containsKey(uri)) {
                return;
            }

            bundledMetadata = queuedMetadata.get(uri);
            queuedMetadata.remove(uri);
        }

        insertMetadataBundleForUri(uri, bundledMetadata);
    }

    private void insertMetadataBundleForUri(String uri, Bundle bundledMetadata) {
        final boolean hasImage = bundledMetadata.getBoolean(KEY_HAS_IMAGE);
        final String metadataJSON = bundledMetadata.getString(KEY_METADATA_JSON);

        // Acquire CPC, must be released in this function.
        final ContentProviderClient contentProviderClient = GeckoAppShell.getApplicationContext()
                .getContentResolver()
                .acquireContentProviderClient(BrowserContract.PageMetadata.CONTENT_URI);

        // Pre-conditions...
        if (contentProviderClient == null) {
            Log.e(LOG_TAG, "Couldn't acquire content provider client");
            return;
        }

        if (TextUtils.isEmpty(metadataJSON)) {
            Log.e(LOG_TAG, "Metadata bundle contained empty metadata json");
            return;
        }

        // Insert!
        try {
            add(
                    BrowserDB.from(GeckoThread.getActiveProfile()),
                    contentProviderClient,
                    uri, hasImage, metadataJSON
            );
        } finally {
            contentProviderClient.release();
        }
    }

    private String prepareJSON(String json) throws JSONException {
        return (new JSONObject(json)).toString();
    }
}
