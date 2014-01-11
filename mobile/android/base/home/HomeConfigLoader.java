/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.home.HomeConfig.PanelConfig;
import org.mozilla.gecko.home.HomeConfig.OnChangeListener;

import android.content.Context;
import android.support.v4.content.AsyncTaskLoader;

import java.util.List;

public class HomeConfigLoader extends AsyncTaskLoader<List<PanelConfig>> {
    private final HomeConfig mConfig;
    private List<PanelConfig> mPanelConfigs;

    public HomeConfigLoader(Context context, HomeConfig homeConfig) {
        super(context);
        mConfig = homeConfig;
    }

    @Override
    public List<PanelConfig> loadInBackground() {
        return mConfig.load();
    }

    @Override
    public void deliverResult(List<PanelConfig> panelConfigs) {
        if (isReset()) {
            mPanelConfigs = null;
            return;
        }

        mPanelConfigs = panelConfigs;
        mConfig.setOnChangeListener(new ForceLoadChangeListener());

        if (isStarted()) {
            super.deliverResult(panelConfigs);
        }
    }

    @Override
    protected void onStartLoading() {
        if (mPanelConfigs != null) {
            deliverResult(mPanelConfigs);
        }

        if (takeContentChanged() || mPanelConfigs == null) {
            forceLoad();
        }
    }

    @Override
    protected void onStopLoading() {
        cancelLoad();
    }

    @Override
    public void onCanceled(List<PanelConfig> panelConfigs) {
        mPanelConfigs = null;
    }

    @Override
    protected void onReset() {
        super.onReset();

        // Ensure the loader is stopped.
        onStopLoading();

        mPanelConfigs = null;
        mConfig.setOnChangeListener(null);
    }

    private class ForceLoadChangeListener implements OnChangeListener {
        @Override
        public void onChange() {
            onContentChanged();
        }
    }
}
