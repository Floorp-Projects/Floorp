/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.OnReloadListener;

import android.content.Context;
import android.support.v4.content.AsyncTaskLoader;

public class HomeConfigLoader extends AsyncTaskLoader<HomeConfig.State> {
    private final HomeConfig mConfig;
    private HomeConfig.State mConfigState;

    private final Context mContext;

    public HomeConfigLoader(Context context, HomeConfig homeConfig) {
        super(context);
        mContext = context;
        mConfig = homeConfig;
    }

    @Override
    public HomeConfig.State loadInBackground() {
        return mConfig.load();
    }

    @Override
    public void deliverResult(HomeConfig.State configState) {
        if (isReset()) {
            mConfigState = null;
            return;
        }

        mConfigState = configState;
        mConfig.setOnReloadListener(new ForceReloadListener());

        if (isStarted()) {
            super.deliverResult(configState);
        }
    }

    @Override
    protected void onStartLoading() {
        if (mConfigState != null) {
            deliverResult(mConfigState);
        }

        if (takeContentChanged() || mConfigState == null) {
            forceLoad();
        }
    }

    @Override
    protected void onStopLoading() {
        cancelLoad();
    }

    @Override
    public void onCanceled(HomeConfig.State configState) {
        mConfigState = null;
    }

    @Override
    protected void onReset() {
        super.onReset();

        // Ensure the loader is stopped.
        onStopLoading();

        mConfigState = null;
        mConfig.setOnReloadListener(null);
    }

    private class ForceReloadListener implements OnReloadListener {
        @Override
        public void onReload() {
            onContentChanged();
        }
    }
}
