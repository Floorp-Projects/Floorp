/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.mozilla.gecko.mozglue.SafeIntent;

import static org.mozilla.gecko.Tabs.INTENT_EXTRA_SESSION_UUID;
import static org.mozilla.gecko.Tabs.INTENT_EXTRA_TAB_ID;
import static org.mozilla.gecko.Tabs.INVALID_TAB_ID;

public abstract class SingleTabActivity extends GeckoApp {
    @Override
    public void onCreate(Bundle savedInstanceState) {
        final Intent externalIntent = getIntent();
        // We need the current activity to already be up-to-date before
        // calling into the superclass.
        GeckoActivityMonitor.getInstance().setCurrentActivity(this);

        decideTabAction(new SafeIntent(externalIntent), savedInstanceState);

        super.onCreate(savedInstanceState);
        // GeckoApp's default behaviour is to reset the intent if we've got any
        // savedInstanceState, which we don't want here.
        setIntent(externalIntent);
    }

    @Override
    protected void onNewIntent(Intent externalIntent) {
        final SafeIntent intent = new SafeIntent(externalIntent);
        // We need the current activity to already be up-to-date before
        // calling into the superclass.
        GeckoActivityMonitor.getInstance().setCurrentActivity(this);

        if (decideTabAction(intent, null)) {
            // GeckoApp will handle tab selection.
            super.onNewIntent(intent.getUnsafe());
        } else {
            // We're not calling the superclass in this code path, so we'll
            // have to notify the activity monitor ourselves.
            GeckoActivityMonitor.getInstance().onActivityNewIntent(this);
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
                tabs.currentActivityMatchesTab(tabToSelect)) {
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
            if (tabs.currentActivityMatchesTab(tabToSelect)) {
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
                tabs.currentActivityMatchesTab(tabToSelect)) {
            intent.getUnsafe().putExtra(INTENT_EXTRA_TAB_ID, lastSelectedTabId);
            intent.getUnsafe().putExtra(INTENT_EXTRA_SESSION_UUID, lastSessionUUID);
            return true;
        }

        // If we end up here, this means that there's no suitable tab we can take over.
        // Instead, we'll just open a new tab from the data specified in the intent.
        return false;
    }

    @Override
    protected void onDone() {
        // Our startup logic should be robust enough to cope with it's tab having been closed even
        // though the activity might survive, so we don't have to call finish() just to make sure
        // that a new tab is opened in that case. This also has the advantage that we'll remain in
        // memory as long as the low-memory killer permits, so we can potentially avoid a costly
        // re-startup of Gecko if the user returns to us soon.
        moveTaskToBack(true);
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
    }
}
