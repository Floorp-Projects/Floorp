/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.icons;

import android.content.Context;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.R;

import java.util.Iterator;
import java.util.TreeSet;
import java.util.concurrent.Future;

/**
 * A class describing a request to load an icon for a website.
 */
public class IconRequest {
    private Context context;

    // Those values are written by the IconRequestBuilder class.
    /* package-private */ String pageUrl;
    /* package-private */ boolean privileged;
    /* package-private */ TreeSet<IconDescriptor> icons;
    /* package-private */ boolean skipNetwork;
    /* package-private */ boolean backgroundThread;
    /* package-private */ boolean skipDisk;
    /* package-private */ boolean skipMemory;
    /* package-private */ int targetSize;
    /* package-private */ boolean prepareOnly;
    private IconCallback callback;

    /* package-private */ IconRequest(Context context) {
        this.context = context.getApplicationContext();
        this.icons = new TreeSet<>(new IconDescriptorComparator());

        // Setting some sensible defaults.
        this.privileged = false;
        this.skipMemory = false;
        this.skipDisk = false;
        this.skipNetwork = false;
        this.targetSize = context.getResources().getDimensionPixelSize(R.dimen.favicon_bg);
        this.prepareOnly = false;
    }

    /**
     * Execute this request and try to load an icon. Once an icon has been loaded successfully the
     * callback will be executed.
     *
     * The returned Future can be used to cancel the job.
     */
    public Future<IconResponse> execute(IconCallback callback) {
        setCallback(callback);

        return IconRequestExecutor.submit(this);
    }

    @VisibleForTesting void setCallback(IconCallback callback) {
        this.callback = callback;
    }

    /**
     * Get the (application) context associated with this request.
     */
    public Context getContext() {
        return context;
    }

    /**
     * Get the descriptor for the potentially best icon. This is the icon that should be loaded if
     * possible.
     */
    public IconDescriptor getBestIcon() {
        return icons.first();
    }

    /**
     * Get the URL of the page for which an icon should be loaded.
     */
    public String getPageUrl() {
        return pageUrl;
    }

    /**
     * Is this request allowed to load icons from internal data sources like the omni.ja?
     */
    public boolean isPrivileged() {
        return privileged;
    }

    /**
     * Get the number of icon descriptors associated with this request.
     */
    public int getIconCount() {
        return icons.size();
    }

    /**
     * Get the required target size of the icon.
     */
    public int getTargetSize() {
        return targetSize;
    }

    /**
     * Should a loader access the network to load this icon?
     */
    public boolean shouldSkipNetwork() {
        return skipNetwork;
    }

    /**
     * Should a loader access the disk to load this icon?
     */
    public boolean shouldSkipDisk() {
        return skipDisk;
    }

    /**
     * Should a loader access the memory cache to load this icon?
     */
    public boolean shouldSkipMemory() {
        return skipMemory;
    }

    /**
     * Get an iterator to iterate over all icon descriptors associated with this request.
     */
    public Iterator<IconDescriptor> getIconIterator() {
        return icons.iterator();
    }

    /**
     * Create a builder to modify this request.
     *
     * Calling methods on the builder will modify this object and not create a copy.
     */
    public IconRequestBuilder modify() {
        return new IconRequestBuilder(this);
    }

    /**
     * Should the callback be executed on a background thread? By default a callback is always
     * executed on the UI thread because an icon is usually loaded in order to display it somewhere
     * in the UI.
     */
    /* package-private */ boolean shouldRunOnBackgroundThread() {
        return backgroundThread;
    }

    /* package-private */ IconCallback getCallback() {
        return callback;
    }

    /* package-private */ boolean hasIconDescriptors() {
        return !icons.isEmpty();
    }

    /**
     * Move to the next icon. This method is called after all loaders for the current best icon
     * have failed. After calling this method getBestIcon() will return the next icon to try.
     * hasIconDescriptors() should be called before requesting the next icon.
     */
    /* package-private */ void moveToNextIcon() {
        icons.remove(getBestIcon());
    }

    /**
     * Should this request be prepared but not actually load an icon?
     */
    /* package-private */ boolean shouldPrepareOnly() {
        return prepareOnly;
    }
}
