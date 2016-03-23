/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.feeds.subscriptions;

import android.content.Context;
import android.text.TextUtils;
import android.util.AtomicFile;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.feeds.FeedFetcher;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Storage for feed subscriptions. This is just using a plain JSON file on disk.
 *
 * TODO: Store this data in the url metadata tablet instead (See bug 1250707)
 */
public class SubscriptionStorage {
    private static final String LOGTAG = "FeedStorage";
    private static final String FILE_NAME = "feed_subscriptions";

    private static final String JSON_KEY_SUBSCRIPTIONS = "subscriptions";

    private final AtomicFile file; // Guarded by 'file'

    private List<FeedSubscription> subscriptions;
    private boolean hasLoadedSubscriptions;
    private boolean hasChanged;

    public SubscriptionStorage(Context context) {
        this(new AtomicFile(new File(context.getApplicationInfo().dataDir, FILE_NAME)));

        startLoadFromDisk();
    }

    // For injecting mocked AtomicFile objects during test
    protected SubscriptionStorage(AtomicFile file) {
        this.subscriptions = new ArrayList<>();
        this.file = file;
    }

    public synchronized void addSubscription(FeedSubscription subscription) {
        awaitLoadingSubscriptionsLocked();

        subscriptions.add(subscription);
        hasChanged = true;
    }

    public synchronized void removeSubscription(FeedSubscription subscription) {
        awaitLoadingSubscriptionsLocked();

        Iterator<FeedSubscription> iterator = subscriptions.iterator();
        while (iterator.hasNext()) {
            if (subscription.isForTheSameBookmarkAs(iterator.next())) {
                iterator.remove();
                hasChanged = true;
                return;
            }
        }
    }

    public synchronized List<FeedSubscription> getSubscriptions() {
        awaitLoadingSubscriptionsLocked();

        return new ArrayList<>(subscriptions);
    }

    public synchronized void updateSubscription(FeedSubscription subscription, FeedFetcher.FeedResponse response) {
        awaitLoadingSubscriptionsLocked();

        subscription.update(response);

        for (int i = 0; i < subscriptions.size(); i++) {
            if (subscriptions.get(i).isForTheSameBookmarkAs(subscription)) {
                subscriptions.set(i, subscription);
                hasChanged = true;
                return;
            }
        }
    }

    public synchronized boolean hasSubscriptionForBookmark(String guid) {
        awaitLoadingSubscriptionsLocked();

        for (int i = 0; i < subscriptions.size(); i++) {
            if (TextUtils.equals(guid, subscriptions.get(i).getBookmarkGUID())) {
                return true;
            }
        }

        return false;
    }

    private void awaitLoadingSubscriptionsLocked() {
        while (!hasLoadedSubscriptions) {
            try {
                Log.v(LOGTAG, "Waiting for subscriptions to be loaded");

                wait();
            } catch (InterruptedException e) {
                // Ignore
            }
        }
    }

    public void persistChanges() {
        new Thread(LOGTAG + "-Persist") {
            public void run() {
                writeToDisk();
            }
        }.start();
    }

    private void startLoadFromDisk() {
        new Thread(LOGTAG + "-Load") {
            public void run() {
                loadFromDisk();
            }
        }.start();
    }

    protected synchronized void loadFromDisk() {
        Log.d(LOGTAG, "Loading from disk");

        if (hasLoadedSubscriptions) {
            return;
        }

        List<FeedSubscription> subscriptions = new ArrayList<>();

        try {
            JSONObject data;

            synchronized (file) {
                data = new JSONObject(new String(file.readFully(), "UTF-8"));
            }

            JSONArray array = data.getJSONArray(JSON_KEY_SUBSCRIPTIONS);
            for (int i = 0; i < array.length(); i++) {
                subscriptions.add(FeedSubscription.fromJSON(array.getJSONObject(i)));
            }
        } catch (FileNotFoundException e) {
            Log.d(LOGTAG, "No subscriptions yet.");
        } catch (JSONException e) {
            Log.w(LOGTAG, "Unable to parse subscriptions JSON. Using empty list.", e);
        } catch (UnsupportedEncodingException e) {
            AssertionError error = new AssertionError("Should not happen: This device does not speak UTF-8");
            error.initCause(e);
            throw error;
        } catch (IOException e) {
            Log.d(LOGTAG, "Can't read subscriptions due to IOException", e);
        }

        onSubscriptionsLoaded(subscriptions);

        notifyAll();

        Log.d(LOGTAG, "Loaded " + subscriptions.size() + " elements");
    }

    protected void onSubscriptionsLoaded(List<FeedSubscription> subscriptions) {
        this.subscriptions = subscriptions;
        this.hasLoadedSubscriptions = true;
    }

    protected synchronized void writeToDisk() {
        if (!hasChanged) {
            Log.v(LOGTAG, "Not persisting: Subscriptions have not changed");
            return;
        }

        Log.d(LOGTAG, "Writing to disk");

        FileOutputStream outputStream = null;

        synchronized (file) {
            try {
                outputStream = file.startWrite();

                JSONArray array = new JSONArray();
                for (FeedSubscription subscription : this.subscriptions) {
                    array.put(subscription.toJSON());
                }

                JSONObject catalog = new JSONObject();
                catalog.put(JSON_KEY_SUBSCRIPTIONS, array);

                outputStream.write(catalog.toString().getBytes("UTF-8"));

                file.finishWrite(outputStream);

                hasChanged = false;
            } catch (UnsupportedEncodingException e) {
                AssertionError error = new AssertionError("Should not happen: This device does not speak UTF-8");
                error.initCause(e);
                throw error;
            } catch (IOException | JSONException e) {
                Log.e(LOGTAG, "IOException during writing catalog", e);

                if (outputStream != null) {
                    file.failWrite(outputStream);
                }
            }
        }
    }
}
