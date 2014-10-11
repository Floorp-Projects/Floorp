/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumMap;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.R;
import org.mozilla.gecko.fxa.AccountLoader;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.login.State;
import org.mozilla.gecko.fxa.login.State.Action;
import org.mozilla.gecko.sync.SyncConstants;

import android.accounts.Account;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * A <code>HomeFragment</code> that, depending on the state of accounts on the
 * device:
 * <ul>
 * <li>displays remote tabs from other devices;</li>
 * <li>offers to re-connect a Firefox Account;</li>
 * <li>offers to create a new Firefox Account.</li>
 * </ul>
 */
public class RemoteTabsPanel extends HomeFragment {
    private static final String LOGTAG = "GeckoRemoteTabsPanel";

    // Loader ID for Android Account loader.
    private static final int LOADER_ID_ACCOUNT = 0;

    // Callback for loaders.
    private AccountLoaderCallbacks mAccountLoaderCallbacks;

    // The current fragment being shown to reflect the system account state. We
    // don't want to detach and re-attach panels unnecessarily, because that
    // causes flickering.
    private Fragment mCurrentFragment;

    // A lazily-populated cache of fragments corresponding to the possible
    // system account states. We don't want to re-create panels unnecessarily,
    // because that can cause flickering. Be aware that null is a valid key; it
    // corresponds to "no Account, neither Firefox nor Legacy Sync."
    private final Map<Action, Fragment> mFragmentCache = new EnumMap<>(Action.class);

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_remote_tabs_panel, container, false);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Create callbacks before the initial loader is started.
        mAccountLoaderCallbacks = new AccountLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void load() {
        getLoaderManager().initLoader(LOADER_ID_ACCOUNT, null, mAccountLoaderCallbacks);
    }

    private void showSubPanel(Fragment subPanel) {
        if (mCurrentFragment == subPanel) {
            return;
        }
        mCurrentFragment = subPanel;

        Bundle args = subPanel.getArguments();
        if (args == null) {
            args = new Bundle();
        }
        args.putBoolean(HomePager.CAN_LOAD_ARG, getCanLoadHint());
        subPanel.setArguments(args);

        getChildFragmentManager()
            .beginTransaction()
            .addToBackStack(null)
            .replace(R.id.remote_tabs_container, subPanel)
            .commitAllowingStateLoss();
    }

    /**
     * Get whatever <code>Action</code> is required to continue healthy syncing
     * of Remote Tabs.
     * <p>
     * A Firefox Account can be in many states, from healthy to requiring a
     * Fennec upgrade to continue use. If we have a Firefox Account, but the
     * state seems corrupt, the best we can do is ask for a password, which
     * resets most of the Account state. The health of a Sync account is
     * essentially opaque in this respect.
     * <p>
     * A null Account means there is no Account (Sync or Firefox) on the device.
     *
     * @param account
     *            Android Account (Sync or Firefox); may be null.
     */
    private Action getActionNeeded(Account account) {
        if (account == null) {
            return null;
        }

        if (SyncConstants.ACCOUNTTYPE_SYNC.equals(account.type)) {
            return Action.None;
        }

        if (!FxAccountConstants.ACCOUNT_TYPE.equals(account.type)) {
            Log.wtf(LOGTAG, "Non Sync, non Firefox Android Account returned by AccountLoader; returning null.");
            return null;
        }

        final State state = FirefoxAccounts.getFirefoxAccountState(getActivity());
        if (state == null) {
            Log.wtf(LOGTAG, "Firefox Account with null state found; offering needs password.");
            return Action.NeedsPassword;
        }

        final Action actionNeeded = state.getNeededAction();
        if (actionNeeded == null) {
            Log.wtf(LOGTAG, "Firefox Account with non-null state but null action needed; offering needs password.");
            return Action.NeedsPassword;
        }

        return actionNeeded;
    }

    private Fragment makeFragmentForAction(Action action) {
        if (action == null) {
            // This corresponds to no Account: neither Sync nor Firefox.
            return RemoteTabsStaticFragment.newInstance(R.layout.remote_tabs_setup);
        }

        switch (action) {
        case None:
            return new RemoteTabsExpandableListFragment();
        case NeedsVerification:
            return RemoteTabsStaticFragment.newInstance(R.layout.remote_tabs_needs_verification);
        case NeedsPassword:
            return RemoteTabsStaticFragment.newInstance(R.layout.remote_tabs_needs_password);
        case NeedsUpgrade:
            return RemoteTabsStaticFragment.newInstance(R.layout.remote_tabs_needs_upgrade);
        default:
            // This should never happen, but we're confident we have a Firefox
            // Account at this point, so let's show the needs password screen.
            // That's our best hope of righting the ship.
            Log.wtf(LOGTAG, "Got unexpected action needed; offering needs password.");
            return RemoteTabsStaticFragment.newInstance(R.layout.remote_tabs_needs_password);
        }
    }

    /**
     * Get the <code>Fragment</code> that reflects the given
     * <code>Account</code> and its state.
     * <p>
     * A null Account means there is no Account (Sync or Firefox) on the device.
     *
     * @param account
     *            Android Account (Sync or Firefox); may be null.
     */
    private Fragment getFragmentNeeded(Account account) {
        final Action actionNeeded = getActionNeeded(account);

        // We use containsKey rather than get because null is a valid key.
        if (!mFragmentCache.containsKey(actionNeeded)) {
            final Fragment fragment = makeFragmentForAction(actionNeeded);
            mFragmentCache.put(actionNeeded, fragment);
        }
        return mFragmentCache.get(actionNeeded);
    }

    /**
     * Update the UI to reflect the given <code>Account</code> and its state.
     * <p>
     * A null Account means there is no Account (Sync or Firefox) on the device.
     *
     * @param account
     *            Android Account (Sync or Firefox); may be null.
     */
    protected void updateUiFromAccount(Account account) {
        if (getView() == null) {
            // Early abort. When the fragment is detached, we get a loader
            // reset, which calls this with a null account parameter. A null
            // account is valid (it means there is no account, either Sync or
            // Firefox), and so we start to offer the setup flow. But this all
            // happens after the view has been destroyed, which means inserting
            // the setup flow fails. In this case, just abort.
            return;
        }
        showSubPanel(getFragmentNeeded(account));
    }

    private class AccountLoaderCallbacks implements LoaderCallbacks<Account> {
        @Override
        public Loader<Account> onCreateLoader(int id, Bundle args) {
            return new AccountLoader(getActivity());
        }

        @Override
        public void onLoadFinished(Loader<Account> loader, Account account) {
            updateUiFromAccount(account);
        }

        @Override
        public void onLoaderReset(Loader<Account> loader) {
            updateUiFromAccount(null);
        }
    }
}
