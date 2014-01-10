/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.toolbar;

import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.util.ThreadUtils;

class ToolbarTitlePrefs {
    public static final String PREF_TITLEBAR_MODE = "browser.chrome.titlebarMode";
    public static final String PREF_TRIM_URLS = "browser.urlbar.trimURLs";

    interface OnChangeListener {
        public void onChange();
    }

    final String[] prefs = {
        PREF_TITLEBAR_MODE,
        PREF_TRIM_URLS
    };

    private boolean mShowUrl;
    private boolean mTrimUrls;

    private Integer mPrefObserverId;

    ToolbarTitlePrefs() {
        mShowUrl = false;
        mTrimUrls = true;

        mPrefObserverId = PrefsHelper.getPrefs(prefs, new TitlePrefsHandler());
    }

    boolean shouldShowUrl() {
        return mShowUrl;
    }

    boolean shouldTrimUrls() {
        return mTrimUrls;
    }

    void close() {
        if (mPrefObserverId != null) {
             PrefsHelper.removeObserver(mPrefObserverId);
             mPrefObserverId = null;
        }
    }

    private class TitlePrefsHandler extends PrefsHelper.PrefHandlerBase {
        @Override
        public void prefValue(String pref, String str) {
            // Handles PREF_TITLEBAR_MODE, which is always a string.
            int value = Integer.parseInt(str);
            boolean shouldShowUrl = (value == 1);

            if (shouldShowUrl == mShowUrl) {
                return;
            }
            mShowUrl = shouldShowUrl;

            triggerChangeListener();
        }

        @Override
        public void prefValue(String pref, boolean value) {
            // Handles PREF_TRIM_URLS, which should usually be a boolean.
            if (value == mTrimUrls) {
                return;
            }
            mTrimUrls = value;

            triggerChangeListener();
        }

        @Override
        public boolean isObserver() {
            // We want to be notified of changes to be able to switch mode
            // without restarting.
            return true;
        }

        private void triggerChangeListener() {
            ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    final Tabs tabs = Tabs.getInstance();
                    final Tab tab = tabs.getSelectedTab();
                    if (tab != null) {
                        tabs.notifyListeners(tab, Tabs.TabEvents.TITLE);
                    }
                }
            });
        }
    }
}
