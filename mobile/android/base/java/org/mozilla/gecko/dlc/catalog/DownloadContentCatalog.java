/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc.catalog;

import android.content.Context;
import android.util.AtomicFile;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Catalog of downloadable content (DLC).
 *
 * Changing elements returned by the catalog should be guarded by the catalog instance to guarantee visibility when
 * persisting changes.
 */
public class DownloadContentCatalog {
    private static final String LOGTAG = "GeckoDLCCatalog";
    private static final String FILE_NAME = "download_content_catalog";

    private static final String JSON_KEY_CONTENT = "content";
    private static final int MAX_FAILURES_UNTIL_PERMANENTLY_FAILED = 10;

    private final AtomicFile file;          // Guarded by 'file'
    private List<DownloadContent> content;  // Guarded by 'this'
    private boolean hasLoadedCatalog;       // Guarded by 'this
    private boolean hasCatalogChanged;      // Guarded by 'this'

    public DownloadContentCatalog(Context context) {
        this(new AtomicFile(new File(context.getApplicationInfo().dataDir, FILE_NAME)));

        startLoadFromDisk();
    }

    // For injecting mocked AtomicFile objects during test
    protected DownloadContentCatalog(AtomicFile file) {
        this.content = Collections.emptyList();
        this.file = file;
    }

    public synchronized List<DownloadContent> getContentWithoutState() {
        awaitLoadingCatalogLocked();

        List<DownloadContent> contentWithoutState = new ArrayList<>();

        for (DownloadContent content : this.content) {
            if (DownloadContent.STATE_NONE == content.getState()) {
                contentWithoutState.add(content);
            }
        }

        return contentWithoutState;
    }

    public synchronized List<DownloadContent> getDownloadedContent() {
        awaitLoadingCatalogLocked();

        List<DownloadContent> downloadedContent = new ArrayList<>();
        for (DownloadContent content : this.content) {
            if (DownloadContent.STATE_DOWNLOADED == content.getState()) {
                downloadedContent.add(content);
            }
        }

        return downloadedContent;
    }

    public synchronized List<DownloadContent> getScheduledDownloads() {
        awaitLoadingCatalogLocked();

        List<DownloadContent> scheduledContent = new ArrayList<>();
        for (DownloadContent content : this.content) {
            if (DownloadContent.STATE_SCHEDULED == content.getState()) {
                scheduledContent.add(content);
            }
        }

        return scheduledContent;
    }

    public synchronized boolean hasScheduledDownloads() {
        awaitLoadingCatalogLocked();

        for (DownloadContent content : this.content) {
            if (DownloadContent.STATE_SCHEDULED == content.getState()) {
                return true;
            }
        }

        return false;
    }

    public synchronized void scheduleDownload(DownloadContent content) {
        content.setState(DownloadContent.STATE_SCHEDULED);
        hasCatalogChanged = true;
    }

    public synchronized void markAsDownloaded(DownloadContent content) {
        content.setState(DownloadContent.STATE_DOWNLOADED);
        content.resetFailures();
        hasCatalogChanged = true;
    }

    public synchronized void markAsPermanentlyFailed(DownloadContent content) {
        content.setState(DownloadContent.STATE_FAILED);
        hasCatalogChanged = true;
    }

    public synchronized void markAsIgnored(DownloadContent content) {
        content.setState(DownloadContent.STATE_IGNORED);
        hasCatalogChanged = true;
    }

    public synchronized void rememberFailure(DownloadContent content, int failureType) {
        if (content.getFailures() >= MAX_FAILURES_UNTIL_PERMANENTLY_FAILED) {
            Log.d(LOGTAG, "Maximum number of failures reached. Marking content has permanently failed.");

            markAsPermanentlyFailed(content);
        } else {
            content.rememberFailure(failureType);
            hasCatalogChanged = true;
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

    private void awaitLoadingCatalogLocked() {
        while (!hasLoadedCatalog) {
            try {
                Log.v(LOGTAG, "Waiting for catalog to be loaded");

                wait();
            } catch (InterruptedException e) {
                // Ignore
            }
        }
    }

    protected synchronized boolean hasCatalogChanged() {
        return hasCatalogChanged;
    }

    protected synchronized void loadFromDisk() {
        Log.d(LOGTAG, "Loading from disk");

        if (hasLoadedCatalog) {
            return;
        }

        List<DownloadContent> content = new ArrayList<>();

        try {
            JSONObject catalog;

            synchronized (file) {
                catalog = new JSONObject(new String(file.readFully(), "UTF-8"));
            }

            JSONArray array = catalog.getJSONArray(JSON_KEY_CONTENT);
            for (int i = 0; i < array.length(); i++) {
                content.add(DownloadContent.fromJSON(array.getJSONObject(i)));
            }
        } catch (FileNotFoundException e) {
            Log.d(LOGTAG, "Catalog file does not exist: Bootstrapping initial catalog");
            content = DownloadContentBootstrap.createInitialDownloadContentList();
        } catch (JSONException e) {
            Log.w(LOGTAG, "Unable to parse catalog JSON. Re-creating catalog.", e);
            // Catalog seems to be broken. Re-create catalog:
            content = DownloadContentBootstrap.createInitialDownloadContentList();
            hasCatalogChanged = true; // Indicate that we want to persist the new catalog
        } catch (UnsupportedEncodingException e) {
            AssertionError error = new AssertionError("Should not happen: This device does not speak UTF-8");
            error.initCause(e);
            throw error;
        } catch (IOException e) {
            Log.d(LOGTAG, "Can't read catalog due to IOException", e);
        }

        onCatalogLoaded(content);

        notifyAll();

        Log.d(LOGTAG, "Loaded " + content.size() + " elements");
    }

    protected void onCatalogLoaded(List<DownloadContent> content) {
        this.content = content;
        this.hasLoadedCatalog = true;
    }

    protected synchronized void writeToDisk() {
        if (!hasCatalogChanged) {
            Log.v(LOGTAG, "Not persisting: Catalog has not changed");
            return;
        }

        Log.d(LOGTAG, "Writing to disk");

        FileOutputStream outputStream = null;

        synchronized (file) {
            try {
                outputStream = file.startWrite();

                JSONArray array = new JSONArray();
                for (DownloadContent content : this.content) {
                    array.put(content.toJSON());
                }

                JSONObject catalog = new JSONObject();
                catalog.put(JSON_KEY_CONTENT, array);

                outputStream.write(catalog.toString().getBytes("UTF-8"));

                file.finishWrite(outputStream);

                hasCatalogChanged = false;
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
