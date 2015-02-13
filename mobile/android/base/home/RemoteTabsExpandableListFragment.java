/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.os.Bundle;
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
public class RemoteTabsExpandableListFragment extends RemoteTabsBaseFragment {
    // Logging tag name.
    private static final String LOGTAG = "GeckoRemoteTabsExpList";

    // The view shown by the fragment.
    private HomeExpandableListView mList;

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
        super.onViewCreated(view, savedInstanceState);

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
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // There is an unfortunate interaction between ExpandableListViews and
        // footer onClick handling. The footer view itself appears to not
        // receive click events. Its children, however, do receive click events.
        // Therefore, we attach an onClick handler to a child of the footer view
        // itself.
        mFooterView = LayoutInflater.from(getActivity()).inflate(R.layout.home_remote_tabs_hidden_devices_footer, mList, false);
        final View view = mFooterView.findViewById(R.id.hidden_devices);
        view.setClickable(true);
        view.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final RemoteClientsDialogFragment dialog = RemoteClientsDialogFragment.newInstance(
                        getResources().getString(R.string.home_remote_tabs_hidden_devices_title),
                        getResources().getString(R.string.home_remote_tabs_unhide_selected_devices),
                        RemoteClientsDialogFragment.ChoiceMode.MULTIPLE, new ArrayList<>(mHiddenClients));
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
        mAdapter = new RemoteTabsExpandableListAdapter(R.layout.home_remote_tabs_group, R.layout.home_remote_tabs_child, null, true);
        mList.setAdapter(mAdapter);

        // Now the adapter is wrapped; we can remove our footer view.
        mList.removeFooterView(mFooterView);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void updateUiFromClients(List<RemoteClient> clients, List<RemoteClient> hiddenClients) {
        if (getView() == null) {
            // Early abort. It is possible to get UI updates after the view is
            // destroyed; this can happen due to asynchronous loaders or
            // animations complete.
            return;
        }

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
}
