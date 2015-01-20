/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.accounts.Account;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.ContextMenu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment.RemoteClientsListener;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Fragment backed by <code>ExpandableListAdapter<code> to displays tabs from other devices.
 */
public abstract class RemoteTabsBaseFragment extends HomeFragment implements RemoteClientsListener {
    // Logging tag name.
    private static final String LOGTAG = "GeckoRemoteTabsBaseFragment";

    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "clients", "tabs" };

    // Cursor loader ID.
    protected static final int LOADER_ID_REMOTE_TABS = 0;

    // Dialog fragment TAG.
    protected static final String DIALOG_TAG_REMOTE_TABS = "dialog_tag_remote_tabs";

    // Maintain group collapsed and hidden state.
    // Only accessed from the UI thread.
    protected static RemoteTabsExpandableListState sState;

    // Adapter for the list of remote tabs.
    protected RemoteTabsExpandableListAdapter mAdapter;

    // List of hidden remote clients.
    // Only accessed from the UI thread.
    protected final List<RemoteClient> mHiddenClients = new ArrayList<>();

    // Callbacks used for the loader.
    protected CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Child refresh layout view.
    protected GeckoSwipeRefreshLayout mRefreshLayout;

    // Sync listener that stops refreshing when a sync is completed.
    protected RemoteTabsSyncListener mSyncStatusListener;

    // Reference to the View to display when there are no results.
    protected View mEmptyView;

    // The footer view to display when there are hidden devices not shown.
    protected View mFooterView;

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mRefreshLayout = (GeckoSwipeRefreshLayout) view.findViewById(R.id.remote_tabs_refresh_layout);
        mRefreshLayout.setColorScheme(
                R.color.swipe_refresh_orange, R.color.swipe_refresh_white,
                R.color.swipe_refresh_orange, R.color.swipe_refresh_white);
        mRefreshLayout.setOnRefreshListener(new RemoteTabsRefreshListener());

        mSyncStatusListener = new RemoteTabsSyncListener();
        FirefoxAccounts.addSyncStatusListener(mSyncStatusListener);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // This races when multiple Fragments are created. That's okay: one
        // will win, and thereafter, all will be okay. If we create and then
        // drop an instance the shared SharedPreferences backing all the
        // instances will maintain the state for us. Since everything happens on
        // the UI thread, this doesn't even need to be volatile.
        if (sState == null) {
            sState = new RemoteTabsExpandableListState(GeckoSharedPrefs.forProfile(getActivity()));
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        if (mSyncStatusListener != null) {
            FirefoxAccounts.removeSyncStatusListener(mSyncStatusListener);
            mSyncStatusListener = null;
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenu.ContextMenuInfo menuInfo) {
        if (!(menuInfo instanceof RemoteTabsClientContextMenuInfo)) {
            // Long pressed item was not a RemoteTabsGroup item. Superclass
            // can handle this.
            super.onCreateContextMenu(menu, view, menuInfo);
            return;
        }

        // Long pressed item was a remote client; provide the appropriate menu.
        final MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.home_remote_tabs_client_contextmenu, menu);

        final RemoteTabsClientContextMenuInfo info = (RemoteTabsClientContextMenuInfo) menuInfo;
        menu.setHeaderTitle(info.client.name);

        // Hide unused menu items.
        final boolean isHidden = sState.isClientHidden(info.client.guid);
        final MenuItem item = menu.findItem(isHidden
                ? R.id.home_remote_tabs_hide_client
                : R.id.home_remote_tabs_show_client);
        item.setVisible(false);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (super.onContextItemSelected(item)) {
            // HomeFragment was able to handle to selected item.
            return true;
        }

        final ContextMenu.ContextMenuInfo menuInfo = item.getMenuInfo();
        if (!(menuInfo instanceof RemoteTabsClientContextMenuInfo)) {
            return false;
        }

        final RemoteTabsClientContextMenuInfo info = (RemoteTabsClientContextMenuInfo) menuInfo;

        final int itemId = item.getItemId();
        if (itemId == R.id.home_remote_tabs_hide_client) {
            sState.setClientHidden(info.client.guid, true);
            getLoaderManager().restartLoader(LOADER_ID_REMOTE_TABS, null, mCursorLoaderCallbacks);
            return true;
        }

        if (itemId == R.id.home_remote_tabs_show_client) {
            sState.setClientHidden(info.client.guid, false);
            getLoaderManager().restartLoader(LOADER_ID_REMOTE_TABS, null, mCursorLoaderCallbacks);
            return true;
        }

        return false;
    }

    @Override
    public void onClients(List<RemoteClient> clients) {
        // The clients listed were hidden and have been checked by the user. We
        // interpret that as "show these clients now".
        for (RemoteClient client : clients) {
            sState.setClientHidden(client.guid, false);
            // There's no particular need to do this, but if you want to see it,
            // let's show it all.
            sState.setClientCollapsed(client.guid, false);
        }
        getLoaderManager().restartLoader(LOADER_ID_REMOTE_TABS, null, mCursorLoaderCallbacks);
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_REMOTE_TABS, null, mCursorLoaderCallbacks);
    }

    protected abstract void updateUiFromClients(List<RemoteClient> clients, List<RemoteClient> hiddenClients);

    private static class RemoteTabsCursorLoader extends SimpleCursorLoader {
        private final GeckoProfile mProfile;

        public RemoteTabsCursorLoader(Context context) {
            super(context);
            mProfile = GeckoProfile.get(context);
        }

        @Override
        public Cursor loadCursor() {
            return mProfile.getDB().getTabsAccessor().getRemoteTabsCursor(getContext());
        }
    }

    protected class CursorLoaderCallbacks extends TransitionAwareCursorLoaderCallbacks {
        private BrowserDB mDB;    // Pseudo-final: set in onCreateLoader.

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            mDB = GeckoProfile.get(getActivity()).getDB();
            return new RemoteTabsCursorLoader(getActivity());
        }

        @Override
        public void onLoadFinishedAfterTransitions(Loader<Cursor> loader, Cursor c) {
            final List<RemoteClient> clients = mDB.getTabsAccessor().getClientsFromCursor(c);

            // Filter the hidden clients out of the clients list. The clients
            // list is updated in place; the hidden clients list is built
            // incrementally.
            mHiddenClients.clear();
            final Iterator<RemoteClient> it = clients.iterator();
            while (it.hasNext()) {
                final RemoteClient client = it.next();
                if (sState.isClientHidden(client.guid)) {
                    it.remove();
                    mHiddenClients.add(client);
                }
            }

            mAdapter.replaceClients(clients);
            updateUiFromClients(clients, mHiddenClients);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            super.onLoaderReset(loader);
            mAdapter.replaceClients(null);
        }
    }

    protected class RemoteTabsRefreshListener implements GeckoSwipeRefreshLayout.OnRefreshListener {
        @Override
        public void onRefresh() {
            if (FirefoxAccounts.firefoxAccountsExist(getActivity())) {
                final Account account = FirefoxAccounts.getFirefoxAccount(getActivity());
                FirefoxAccounts.requestSync(account, FirefoxAccounts.FORCE, STAGES_TO_SYNC_ON_REFRESH, null);
            } else {
                Log.wtf(LOGTAG, "No Firefox Account found; this should never happen. Ignoring.");
                mRefreshLayout.setRefreshing(false);
            }
        }
    }

    protected class RemoteTabsSyncListener implements FirefoxAccounts.SyncStatusListener {
        @Override
        public Context getContext() {
            return getActivity();
        }

        @Override
        public Account getAccount() {
            return FirefoxAccounts.getFirefoxAccount(getContext());
        }

        @Override
        public void onSyncStarted() {
        }

        @Override
        public void onSyncFinished() {
            mRefreshLayout.setRefreshing(false);
        }
    }

    /**
     * Stores information regarding the creation of the context menu for a remote client.
     */
    protected static class RemoteTabsClientContextMenuInfo extends HomeContextMenuInfo {
        protected final RemoteClient client;

        public RemoteTabsClientContextMenuInfo(View targetView, int position, long id, RemoteClient client) {
            super(targetView, position, id);
            this.client = client;
        }
    }
}
