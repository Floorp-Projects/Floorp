/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.R;
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
import android.view.LayoutInflater;
import android.view.View;
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
public class RemoteTabsExpandableListFragment extends HomeFragment {
    // Logging tag name.
    private static final String LOGTAG = "GeckoRemoteTabsExpList";

    // Cursor loader ID.
    private static final int LOADER_ID_REMOTE_TABS = 0;

    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "clients", "tabs" };

    // Adapter for the list of remote tabs.
    private RemoteTabsExpandableListAdapter mAdapter;

    // The view shown by the fragment.
    private HomeExpandableListView mList;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

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
                // Since we don't indicate the expansion state yet, don't allow
                // collapsing groups at all.
                return true;
            }
        });

        // Show a context menu only for tabs (not for clients).
        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.ExpandableFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ExpandableListAdapter adapter) {
                long packedPosition = mList.getExpandableListPosition(position);
                if (ExpandableListView.getPackedPositionType(packedPosition) != ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
                    return null;
                }
                final int groupPosition = ExpandableListView.getPackedPositionGroup(packedPosition);
                final int childPosition = ExpandableListView.getPackedPositionChild(packedPosition);
                final Object child = adapter.getChild(groupPosition, childPosition);
                if (child instanceof RemoteTab) {
                    final RemoteTab tab = (RemoteTab) child;
                    final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                    info.url = tab.url;
                    info.title = tab.title;
                    return info;
                } else {
                    return null;
                }
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

        // Intialize adapter
        mAdapter = new RemoteTabsExpandableListAdapter(R.layout.home_remote_tabs_group, R.layout.home_remote_tabs_child, null);
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    private void updateUiFromClients(List<RemoteClient> clients) {
        if (clients != null && !clients.isEmpty()) {
            for (int i = 0; i < mList.getExpandableListAdapter().getGroupCount(); i++) {
                mList.expandGroup(i);
            }
            return;
        }

        // Cursor is empty, so set the empty view if it hasn't been set already.
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
            mAdapter.replaceClients(clients);
            updateUiFromClients(clients);
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

        public void onSyncStarted() {
        }

        public void onSyncFinished() {
            mRefreshLayout.setRefreshing(false);
        }
    }
}
