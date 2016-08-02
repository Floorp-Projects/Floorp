/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.promotion;

import android.content.Intent;
import android.content.SharedPreferences;

import com.keepsafe.switchboard.SwitchBoard;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.delegates.BrowserAppDelegateWithReference;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.Experiments;

public class ReaderViewBookmarkPromotion extends BrowserAppDelegateWithReference implements Tabs.OnTabsChangedListener {
    private static final String PREF_FIRST_RV_HINT_SHOWN = "first_reader_view_hint_shown";
    private static final String FIRST_READERVIEW_OPEN_TELEMETRYEXTRA = "first_readerview_open_prompt";

    private boolean hasEnteredReaderMode = false;

    @Override
    public void onResume(BrowserApp browserApp) {
        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        Tabs.unregisterOnTabsChangedListener(this);
    }

    @Override
    public void onTabChanged(Tab tab, Tabs.TabEvents msg, String data) {
        switch (msg) {
            case LOCATION_CHANGE:
                // old url: data
                // new url: tab.getURL()
                final boolean enteringReaderMode = ReaderModeUtils.isEnteringReaderMode(tab.getURL(), data);

                if (!hasEnteredReaderMode && enteringReaderMode) {
                    hasEnteredReaderMode = true;
                    promoteBookmarking();
                }

                break;
        }
    }

    @Override
    public void onActivityResult(BrowserApp browserApp, int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case BrowserApp.ACTIVITY_REQUEST_TRIPLE_READERVIEW:
                if (resultCode == BrowserApp.ACTIVITY_RESULT_TRIPLE_READERVIEW_ADD_BOOKMARK) {
                    final Tab tab = Tabs.getInstance().getSelectedTab();
                    if (tab != null) {
                        tab.addBookmark();
                    }
                } else if (resultCode == BrowserApp.ACTIVITY_RESULT_TRIPLE_READERVIEW_IGNORE) {
                    // Nothing to do: we won't show this promotion again either way.
                }
                break;
        }
    }

    private void promoteBookmarking() {
        final BrowserApp browserApp = getBrowserApp();
        if (browserApp == null) {
            return;
        }

        final SharedPreferences prefs = GeckoSharedPrefs.forProfile(browserApp);
        final boolean isEnabled = SwitchBoard.isInExperiment(browserApp, Experiments.TRIPLE_READERVIEW_BOOKMARK_PROMPT);

        // We reuse the same preference as for the first offline reader view bookmark
        // as we only want to show one of the two UIs (they both explain the same
        // functionality).
        if (!isEnabled || prefs.getBoolean(PREF_FIRST_RV_HINT_SHOWN, false)) {
            return;
        }

        SimpleHelperUI.show(browserApp,
                FIRST_READERVIEW_OPEN_TELEMETRYEXTRA,
                BrowserApp.ACTIVITY_REQUEST_TRIPLE_READERVIEW,
                R.string.helper_triple_readerview_open_title,
                R.string.helper_triple_readerview_open_message,
                R.drawable.helper_readerview_bookmark, // We share the icon with the usual helper UI
                R.string.helper_triple_readerview_open_button,
                BrowserApp.ACTIVITY_RESULT_TRIPLE_READERVIEW_ADD_BOOKMARK,
                BrowserApp.ACTIVITY_RESULT_TRIPLE_READERVIEW_IGNORE);

        prefs
                .edit()
                .putBoolean(PREF_FIRST_RV_HINT_SHOWN, true)
                .apply();
    }

}
