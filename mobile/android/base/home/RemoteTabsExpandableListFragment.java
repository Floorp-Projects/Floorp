/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.Iterator;
import java.util.List;

import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.RemoteClientsDialogFragment.ChoiceMode;
import org.mozilla.gecko.RemoteClientsDialogFragment.RemoteClientsListener;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter;
import org.mozilla.gecko.TabsAccessor;
import org.mozilla.gecko.TabsAccessor.RemoteClient;
import org.mozilla.gecko.TabsAccessor.RemoteTab;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout;
import org.mozilla.gecko.widget.GeckoSwipeRefreshLayout.OnRefreshListener;

import android.accounts.Account;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ExpandableListAdapter;
import android.widget.ExpandableListView;
import android.widget.ExpandableListView.OnChildClickListener;
import android.widget.ExpandableListView.OnGroupClickListener;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Fragment that displays tabs from other devices in an <code>ExpandableListView<code>.
 * <p>
 * This is intended to be used on phones, and possibly in portrait mode on tablets.
 */
public class RemoteTabsExpandableListFragment extends HomeFragment implements RemoteClientsListener {
    // Logging tag name.
    private static final String LOGTAG = "GeckoRemoteTabsExpList";

    // Cursor loader ID.
    private static final int LOADER_ID_REMOTE_TABS = 0;

    // Dialog fragment TAG.
    private static final String DIALOG_TAG_REMOTE_TABS = "dialog_tag_remote_tabs";

    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "clients", "tabs" };

    // Maintain group collapsed and hidden state.
    // Only accessed from the UI thread.
    private static RemoteTabsExpandableListState sState;

    // Adapter for the list of remote tabs.
    private RemoteTabsExpandableListAdapter mAdapter;

    // List of hidden remote clients.
    // Only accessed from the UI thread.
    private final ArrayList<RemoteClient> mHiddenClients = new ArrayList<RemoteClient>();

    // The view shown by the fragment.
    private HomeExpandableListView mList;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // The footer view to display when there are hidden devices not shown.
    private View mFooterView;

    // Callbacks used for the loader.
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Child refresh layout view.
    private GeckoSwipeRefreshLayout mRefreshLayout;

    // Sync listener that stops refreshing when a sync is completed.
    private RemoteTabsSyncListener mSyncStatusListener;

    public static RemoteTabsExpandableListFragment newInstance() {
        return new RemoteTabsExpandableListFragment();
    }

    public RemoteTabsExpandableListFragment() {
        super();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_remote_tabs_list_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mRefreshLayout = (GeckoSwipeRefreshLayout) view.findViewById(R.id.remote_tabs_refresh_layout);
        mRefreshLayout.setColorScheme(
                R.color.swipe_refresh_orange, R.color.swipe_refresh_white,
                R.color.swipe_refresh_orange, R.color.swipe_refresh_white);
        mRefreshLayout.setOnRefreshListener(new RemoteTabsRefreshListener());

        mSyncStatusListener = new RemoteTabsSyncListener();
        FirefoxAccounts.addSyncStatusListener(mSyncStatusListener);

        mList = (HomeExpandableListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_REMOTE_TABS);

        mList.setOnChildClickListener(new OnChildClickListener() {
            @Override
            public boolean onChildClick(ExpandableListView parent, View v, int groupPosition, int childPosition, long id) {
                final ExpandableListAdapter adapter = parent.getExpandableListAdapter();
                final RemoteTab tab = (RemoteTab) adapter.getChild(groupPosition, childPosition);
                if (tab == null) {
                    return false;
                }

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM);

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(tab.url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
                return true;
            }
        });

        mList.setOnGroupClickListener(new OnGroupClickListener() {
            @Override
            public boolean onGroupClick(ExpandableListView parent, View v, int groupPosition, long id) {
                final ExpandableListAdapter adapter = parent.getExpandableListAdapter();
                final RemoteClient client = (RemoteClient) adapter.getGroup(groupPosition);
                if (client != null) {
                    // After we process this click, the group's expanded state will have flipped.
                    sState.setClientCollapsed(client.guid, mList.isGroupExpanded(groupPosition));
                }

                // We want the system to handle the click, expanding or collapsing as necessary.
                return false;
            }
        });

        // Show a context menu only for tabs (not for clients).
        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.ExpandableFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ExpandableListAdapter adapter) {
                long packedPosition = mList.getExpandableListPosition(position);
                final int groupPosition = ExpandableListView.getPackedPositionGroup(packedPosition);
                final int type = ExpandableListView.getPackedPositionType(packedPosition);
                if (type == ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
                    final int childPosition = ExpandableListView.getPackedPositionChild(packedPosition);
                    final RemoteTab tab = (RemoteTab) adapter.getChild(groupPosition, childPosition);
                    final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                    info.url = tab.url;
                    info.title = tab.title;
                    return info;
                }

                if (type == ExpandableListView.PACKED_POSITION_TYPE_GROUP) {
                    final RemoteClient client = (RemoteClient) adapter.getGroup(groupPosition);
                    final RemoteTabsClientContextMenuInfo info = new RemoteTabsClientContextMenuInfo(view, position, id, client);
                    return info;
                }
                return null;
            }
        });

        registerForContextMenu(mList);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mEmptyView = null;

        if (mSyncStatusListener != null) {
            FirefoxAccounts.removeSyncStatusListener(mSyncStatusListener);
            mSyncStatusListener = null;
        }
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

        // There is an unfortunate interaction between ExpandableListViews and
        // footer onClick handling. The footer view itself appears to not
        // receive click events. Its children, however, do receive click events.
        // Therefore, we attach an onClick handler to a child of the footer view
        // itself.
        mFooterView = LayoutInflater.from(getActivity()).inflate(R.layout.home_remote_tabs_hidden_devices_footer, mList, false);
        final View view = mFooterView.findViewById(R.id.hidden_devices);
        view.setClickable(true);
        view.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                final RemoteClientsDialogFragment dialog = RemoteClientsDialogFragment.newInstance(
                        getResources().getString(R.string.home_remote_tabs_hidden_devices_title),
                        getResources().getString(R.string.home_remote_tabs_unhide_selected_devices),
                        ChoiceMode.MULTIPLE, new ArrayList<RemoteClient>(mHiddenClients));
                dialog.setTargetFragment(RemoteTabsExpandableListFragment.this, 0);
                dialog.show(getActivity().getSupportFragmentManager(), DIALOG_TAG_REMOTE_TABS);
            }
        });

        // There is a delicate interaction, pre-KitKat, between
        // {add,remove}FooterView and setAdapter. setAdapter wraps the adapter
        // in a footer/header-managing adapter, which only happens (pre-KitKat)
        // if a footer/header is present. Therefore, we add our footer before
        // setting the adapter; and then we remove it afterward. From there on,
        // we can add/remove it at will.
        mList.addFooterView(mFooterView, null, true);

        // Initialize adapter
        mAdapter = new RemoteTabsExpandableListAdapter(R.layout.home_remote_tabs_group, R.layout.home_remote_tabs_child, null);
        mList.setAdapter(mAdapter);

        // Now the adapter is wrapped; we can remove our footer view.
        mList.removeFooterView(mFooterView);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenuInfo menuInfo) {
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

        final ContextMenuInfo menuInfo = item.getMenuInfo();
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

    private void updateUiFromClients(List<RemoteClient> clients, List<RemoteClient> hiddenClients) {
        // We have three states: no clients (including hidden clients) at all;
        // all clients hidden; some clients hidden. We want to show the empty
        // list view only when we have no clients at all. This flag
        // differentiates the first from the latter two states.
        boolean displayedSomeClients = false;

        if (hiddenClients == null || hiddenClients.isEmpty()) {
            mList.removeFooterView(mFooterView);
        } else {
            displayedSomeClients = true;

            final TextView textView = (TextView) mFooterView.findViewById(R.id.hidden_devices);
            if (hiddenClients.size() == 1) {
                textView.setText(getResources().getString(R.string.home_remote_tabs_one_hidden_device));
            } else {
                textView.setText(getResources().getString(R.string.home_remote_tabs_many_hidden_devices, hiddenClients.size()));
            }

            // This is a simple, if not very future-proof, way to determine if
            // the footer view has already been added to the list view.
            if (mList.getFooterViewsCount() < 1) {
                mList.addFooterView(mFooterView);
            }
        }

        if (clients != null && !clients.isEmpty()) {
            displayedSomeClients = true;

            // No sense crashing if we've made an error.
            int groupCount = Math.min(mList.getExpandableListAdapter().getGroupCount(), clients.size());
            for (int i = 0; i < groupCount; i++) {
                final RemoteClient client = clients.get(i);
                if (sState.isClientCollapsed(client.guid)) {
                    mList.collapseGroup(i);
                } else {
                    mList.expandGroup(i);
                }
            }
        }

        if (displayedSomeClients) {
            return;
        }

        // No clients shown, not even hidden clients. Set the empty view if it
        // hasn't been set already.
        if (mEmptyView == null) {
            // Set empty panel view. We delay this so that the empty view won't flash.
            final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_remote_tabs_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_remote_tabs_empty);

            mList.setEmptyView(mEmptyView);
        }
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

    private static class RemoteTabsCursorLoader extends SimpleCursorLoader {
        public RemoteTabsCursorLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            return TabsAccessor.getRemoteTabsCursor(getContext());
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new RemoteTabsCursorLoader(getActivity());
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final List<RemoteClient> clients = TabsAccessor.getClientsFromCursor(c);

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
            mAdapter.replaceClients(null);
        }
    }

    private class RemoteTabsRefreshListener implements OnRefreshListener {
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

    private class RemoteTabsSyncListener implements FirefoxAccounts.SyncStatusListener {
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
