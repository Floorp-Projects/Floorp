/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.ColorInt;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.content.ContextCompat;
import android.view.View;

import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.util.ThreadUtils;

import static org.mozilla.gecko.Tabs.INTENT_EXTRA_SESSION_UUID;
import static org.mozilla.gecko.Tabs.INTENT_EXTRA_TAB_ID;
import static org.mozilla.gecko.Tabs.INVALID_TAB_ID;

public abstract class SingleTabActivity extends GeckoApp {

    private static final long SHOW_CONTENT_DELAY = 300;

    private View mainLayout;
    private View contentView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        final Intent externalIntent = getIntent();

        decideTabAction(new SafeIntent(externalIntent), savedInstanceState);

        super.onCreate(savedInstanceState);
        // GeckoApp's default behaviour is to reset the intent if we've got any
        // savedInstanceState, which we don't want here.
        setIntent(externalIntent);

        mainLayout = findViewById(R.id.main_layout);
        contentView = findViewById(R.id.gecko_layout);
        if ((mainLayout != null) && (contentView != null)) {
            @ColorInt final int bg = ContextCompat.getColor(this, android.R.color.white);
            mainLayout.setBackgroundColor(bg);
            contentView.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    protected void onNewIntent(Intent externalIntent) {
        final SafeIntent intent = new SafeIntent(externalIntent);

        if (decideTabAction(intent, null)) {
            // GeckoApp will handle tab selection.
            super.onNewIntent(intent.getUnsafe());
        } else {
            loadTabFromIntent(intent);
        }
        // Again, unlike GeckoApp's default behaviour we want to keep the intent around
        // because we might still require its data (e.g. to get custom tab customisations).
        setIntent(intent.getUnsafe());
    }

    @Override
    protected boolean saveSelectedStartupTab() {
        // We ignore the tab selection made by session restoring in order to display our own tab,
        // so we should save that tab's ID in case the user starts up our normal browsing UI later
        // during the session.
        return true;
    }

    @Override
    protected void restoreLastSelectedTab() {
        if (!mInitialized) {
            // During startup from onCreate(), initialize() will handle selecting the startup tab.
            // If this here is called afterwards, it's a no-op anyway. If for some reason
            // (e.g. debugging) initialize() takes longer than usual and hasn't finished by the time
            // onResume() runs and calls us, we just exit early so as not to interfere.
            return;
        }

        final Tabs tabs = Tabs.getInstance();
        final Tab tabToSelect = tabs.getTab(mLastSelectedTabId);

        // If the tab we've stored is still existing and valid select it...
        if (tabToSelect != null && GeckoApplication.getSessionUUID().equals(mLastSessionUUID) &&
                tabToSelect.matchesActivity(this)) {
            tabs.selectTab(mLastSelectedTabId);
        } else {
            // ... otherwise fall back to the intent data and open a new tab.
            loadTabFromIntent(new SafeIntent(getIntent()));
        }
    }

    private void loadTabFromIntent(final SafeIntent intent) {
        final int flags = getNewTabFlags();
        loadStartupTab(getIntentURI(intent), intent, flags);
    }

    /**
     * @return True if we're going to select an existing tab, false if we want to load a new tab.
     */
    private boolean decideTabAction(@NonNull final SafeIntent intent,
                                    @Nullable final Bundle savedInstanceState) {
        final Tabs tabs = Tabs.getInstance();

        if (hasGeckoTab(intent)) {
            final Tab tabToSelect = tabs.getTab(intent.getIntExtra(INTENT_EXTRA_TAB_ID, INVALID_TAB_ID));
            if (tabToSelect.matchesActivity(this)) {
                // Nothing further to do here, GeckoApp will select the correct
                // tab from the intent.
                return true;
            }
        }
        // The intent doesn't refer to a valid tab, so don't pass that data on.
        intent.getUnsafe().removeExtra(INTENT_EXTRA_TAB_ID);
        intent.getUnsafe().removeExtra(INTENT_EXTRA_SESSION_UUID);
        // The tab data in the intent can become stale if we've been killed, or have
        // closed the tab/changed its type since the original intent.
        // We therefore attempt to fall back to the last selected tab. In onNewIntent,
        // we can directly use the stored data, otherwise we'll look for it in the
        // savedInstanceState.
        final int lastSelectedTabId;
        final String lastSessionUUID;

        if (savedInstanceState != null) {
            lastSelectedTabId = savedInstanceState.getInt(LAST_SELECTED_TAB);
            lastSessionUUID = savedInstanceState.getString(LAST_SESSION_UUID);
        } else {
            lastSelectedTabId = mLastSelectedTabId;
            lastSessionUUID = mLastSessionUUID;
        }

        final Tab tabToSelect = tabs.getTab(lastSelectedTabId);
        if (tabToSelect != null && GeckoApplication.getSessionUUID().equals(lastSessionUUID) &&
                tabToSelect.matchesActivity(this)) {
            intent.getUnsafe().putExtra(INTENT_EXTRA_TAB_ID, lastSelectedTabId);
            intent.getUnsafe().putExtra(INTENT_EXTRA_SESSION_UUID, lastSessionUUID);
            return true;
        }

        // If we end up here, this means that there's no suitable tab we can take over.
        // Instead, we'll just open a new tab from the data specified in the intent.
        return false;
    }

    /**
     * For us here, mLastSelectedTabId/Hash will hold the tab that will be selected when the
     * activity is resumed/recreated, unless
     * - it has been explicitly overridden through an intent
     * - the tab cannot be found, in which case the URI passed as intent data will instead be
     *   opened in a new tab.
     * Therefore, we only update the stored tab data from those two locations.
     */

    /**
     * Called when an intent or onResume() has caused us to load and select a new tab.
     *
     * @param tab The new tab that has been opened and selected.
     */
    @Override
    protected void onTabOpenFromIntent(Tab tab) {
        mLastSelectedTabId = tab.getId();
        mLastSessionUUID = GeckoApplication.getSessionUUID();
        showContentView();
    }

    /**
     * Called when an intent has caused us to select an already existing tab.
     *
     * @param tab The already existing tab that has been selected for this activity.
     */
    @Override
    protected void onTabSelectFromIntent(Tab tab) {
        mLastSelectedTabId = tab.getId();
        mLastSessionUUID = GeckoApplication.getSessionUUID();
        showContentView();
    }

    // Bug 1369681 - a workaround to prevent flash in first launch.
    // This will be removed once we have GeckoView based implementation.
    private void showContentView() {
        if ((contentView != null)
                && (mainLayout != null)
                && (contentView.getVisibility() == View.INVISIBLE)) {
            ThreadUtils.postDelayedToUiThread(new Runnable() {
                @Override
                public void run() {
                    @ColorInt final int bg =
                            ContextCompat.getColor(mainLayout.getContext(), android.R.color.white);
                    mainLayout.setBackgroundColor(bg);
                    contentView.setVisibility(View.VISIBLE);
                }
            }, SHOW_CONTENT_DELAY);
        }
    }
}
