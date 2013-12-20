/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.PageEntry;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;

import android.content.Context;
import android.support.v4.content.AsyncTaskLoader;

import java.util.List;

public class HomeConfigLoader extends AsyncTaskLoader<List<PageEntry>> {
    private final HomeConfig mConfig;
    private List<PageEntry> mPageEntries;

    public HomeConfigLoader(Context context, HomeConfig homeConfig) {
        super(context);
        mConfig = homeConfig;
    }

    @Override
    public List<PageEntry> loadInBackground() {
        return mConfig.load();
    }

    @Override
    public void deliverResult(List<PageEntry> pageEntries) {
        if (isReset()) {
            mPageEntries = null;
            return;
        }

        mPageEntries = pageEntries;
        mConfig.setOnChangeListener(new ForceLoadChangeListener());

        if (isStarted()) {
            super.deliverResult(pageEntries);
        }
    }

    @Override
    protected void onStartLoading() {
        if (mPageEntries != null) {
            deliverResult(mPageEntries);
        }

        if (takeContentChanged() || mPageEntries == null) {
            forceLoad();
        }
    }

    @Override
    protected void onStopLoading() {
        cancelLoad();
    }

    @Override
    public void onCanceled(List<PageEntry> pageEntries) {
        mPageEntries = null;
    }

    @Override
    protected void onReset() {
        super.onReset();

        // Ensure the loader is stopped.
        onStopLoading();

        mPageEntries = null;
        mConfig.setOnChangeListener(null);
    }

    private class ForceLoadChangeListener implements OnChangeListener {
        @Override
        public void onChange() {
            onContentChanged();
        }
    }
}
