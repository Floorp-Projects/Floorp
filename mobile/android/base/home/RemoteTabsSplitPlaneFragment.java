package org.mozilla.gecko.home;

import android.content.Context;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AbsListView;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.ListAdapter;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter;
import org.mozilla.gecko.RemoteTabsExpandableListAdapter.GroupViewHolder;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;

import java.util.ArrayList;
import java.util.EnumSet;
import java.util.List;

/**
 * Fragment that displays other devices and tabs from them in two separate <code>ListView<code> instances.
 * <p/>
 * This is intended to be used in landscape mode on tablets.
 */
public class RemoteTabsSplitPlaneFragment extends RemoteTabsBaseFragment {
    // Logging tag name.
    private static final String LOGTAG = "GeckoSplitPlaneFragment";

    private ArrayAdapter<RemoteTab> mTabsAdapter;
    private ArrayAdapter<RemoteClient> mClientsAdapter;

    // DataSetObserver for the expandable list adapter.
    private DataSetObserver mObserver;

    // The views shown by the fragment.
    private HomeListView mClientList;
    private HomeListView mTabList;

    public static RemoteTabsSplitPlaneFragment newInstance() {
        return new RemoteTabsSplitPlaneFragment();
    }

    public RemoteTabsSplitPlaneFragment() {
        super();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_remote_tabs_split_plane_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mClientList = (HomeListView) view.findViewById(R.id.clients_list);
        mTabList = (HomeListView) view.findViewById(R.id.tabs_list);

        mClientList.setTag(HomePager.LIST_TAG_REMOTE_TABS);

        mTabList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
                final RemoteTab tab = (RemoteTab) adapter.getItemAtPosition(position);
                if (tab == null) {
                    return;
                }

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM);

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(tab.url, EnumSet.of(HomePager.OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mClientList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
                final RemoteClient client = (RemoteClient) adapter.getItemAtPosition(position);
                if (client != null) {
                    sState.setClientAsSelected(client.guid);
                    mTabsAdapter.clear();
                    for (RemoteTab tab : client.tabs) {
                        mTabsAdapter.add(tab);
                    }

                    // Notify data has changed for both clients and tabs adapter.
                    // This will update selected client item background and the tabs list.
                    mClientsAdapter.notifyDataSetChanged();
                    mTabsAdapter.notifyDataSetChanged();
                }
            }
        });

        mTabList.setContextMenuInfoFactory(new HomeContextMenuInfo.ListFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                return null;
            }

            @Override
            public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ListAdapter adapter) {
                final RemoteTab tab = (RemoteTab) adapter.getItem(position);
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = tab.url;
                info.title = tab.title;
                return info;
            }
        });

        mClientList.setContextMenuInfoFactory(new HomeContextMenuInfo.ListFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                return null;
            }

            @Override
            public HomeContextMenuInfo makeInfoForAdapter(View view, int position, long id, ListAdapter adapter) {
                final RemoteClient client = (RemoteClient) adapter.getItem(position);
                return new RemoteTabsClientContextMenuInfo(view, position, id, client);
            }
        });

        registerForContextMenu(mClientList);
        registerForContextMenu(mTabList);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mClientList = null;
        mTabList = null;
        mEmptyView = null;
        mAdapter.unregisterDataSetObserver(mObserver);
        mObserver = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // There is an unfortunate interaction between ListViews and
        // footer onClick handling. The footer view itself appears to not
        // receive click events. Its children, however, do receive click events.
        // Therefore, we attach an onClick handler to a child of the footer view
        // itself.
        mFooterView = LayoutInflater.from(getActivity()).inflate(R.layout.home_remote_tabs_hidden_devices_footer, mClientList, false);
        final View view = mFooterView.findViewById(R.id.hidden_devices);
        view.setClickable(true);
        view.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final RemoteClientsDialogFragment dialog = RemoteClientsDialogFragment.newInstance(
                        getResources().getString(R.string.home_remote_tabs_hidden_devices_title),
                        getResources().getString(R.string.home_remote_tabs_unhide_selected_devices),
                        RemoteClientsDialogFragment.ChoiceMode.MULTIPLE, new ArrayList<>(mHiddenClients));
                dialog.setTargetFragment(RemoteTabsSplitPlaneFragment.this, 0);
                dialog.show(getActivity().getSupportFragmentManager(), DIALOG_TAG_REMOTE_TABS);
            }
        });

        // There is a delicate interaction, pre-KitKat, between
        // {add,remove}FooterView and setAdapter. setAdapter wraps the adapter
        // in a footer/header-managing adapter, which only happens (pre-KitKat)
        // if a footer/header is present. Therefore, we add our footer before
        // setting the adapter; and then we remove it afterward. From there on,
        // we can add/remove it at will.
        mClientList.addFooterView(mFooterView, null, true);

        // Initialize adapter
        mAdapter = new RemoteTabsExpandableListAdapter(R.layout.home_remote_tabs_group, R.layout.home_remote_tabs_child, null, false);

        mTabsAdapter = new RemoteTabsAdapter(getActivity(), R.layout.home_remote_tabs_child);
        mClientsAdapter = new RemoteClientAdapter(getActivity(), R.layout.home_remote_tabs_group, mAdapter);

        // ArrayAdapter.addAll() is supported only from API 11. We avoid redundant notifications while each item is added to the adapter here.
        // ArrayAdapter notifyDataSetChanged should be called after all add operations manually.
        mTabsAdapter.setNotifyOnChange(false);
        mClientsAdapter.setNotifyOnChange(false);

        mTabList.setAdapter(mTabsAdapter);
        mClientList.setAdapter(mClientsAdapter);

        mObserver = new RemoteTabDataSetObserver();
        mAdapter.registerDataSetObserver(mObserver);

        // Now the adapter is wrapped; we can remove our footer view.
        mClientList.removeFooterView(mFooterView);

        // Register touch handler to conditionally enable swipe refresh layout.
        mClientList.setOnTouchListener(new ListTouchListener(mClientList));
        mTabList.setOnTouchListener(new ListTouchListener(mTabList));

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
            mClientList.removeFooterView(mFooterView);
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
            if (mClientList.getFooterViewsCount() < 1) {
                mClientList.addFooterView(mFooterView);
            }
        }

        if (clients != null && !clients.isEmpty()) {
            displayedSomeClients = true;
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

            mClientList.setEmptyView(mEmptyView);
        }
    }

    private class RemoteTabDataSetObserver extends DataSetObserver {
        @Override
        public void onChanged() {
            super.onChanged();
            mClientsAdapter.clear();
            mTabsAdapter.clear();

            RemoteClient selectedClient = null;
            for (int i = 0; i < mAdapter.getGroupCount(); i++) {
                final RemoteClient client = (RemoteClient) mAdapter.getGroup(i);
                mClientsAdapter.add(client);

                if (i == 0) {
                    // Fallback to most recent client when selected client guid not found.
                    selectedClient = client;
                }

                if (client.guid.equals(sState.selectedClient)) {
                    selectedClient = client;
                }
            }

            final List<RemoteTab> visibleTabs = (selectedClient != null) ? selectedClient.tabs : new ArrayList<RemoteTab>();
            for (RemoteTab tab : visibleTabs) {
                mTabsAdapter.add(tab);
            }

            // Update the selected client and notify data has changed both the list views.
            sState.setClientAsSelected(selectedClient != null ? selectedClient.guid : null);
            mTabsAdapter.notifyDataSetChanged();
            mClientsAdapter.notifyDataSetChanged();
        }

        @Override
        public void onInvalidated() {
            super.onInvalidated();
            mClientsAdapter.clear();
            mTabsAdapter.clear();
            mTabsAdapter.notifyDataSetChanged();
            mClientsAdapter.notifyDataSetChanged();
        }
    }

    private static class RemoteTabsAdapter extends ArrayAdapter<RemoteTab> {
        private final Context context;
        private final int resource;

        public RemoteTabsAdapter(Context context, int resource) {
            super(context, resource);
            this.context = context;
            this.resource = resource;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final TwoLinePageRow view;
            if (convertView != null) {
                view = (TwoLinePageRow) convertView;
            } else {
                final LayoutInflater inflater = LayoutInflater.from(context);
                view = (TwoLinePageRow) inflater.inflate(resource, parent, false);
            }

            final RemoteTab tab = getItem(position);
            view.update(tab.title, tab.url);

            return view;
        }
    }

    private class RemoteClientAdapter extends ArrayAdapter<RemoteClient> {
        private final Context context;
        private final int resource;
        private final RemoteTabsExpandableListAdapter adapter;

        public RemoteClientAdapter(Context context, int resource, RemoteTabsExpandableListAdapter adapter) {
            super(context, resource);
            this.context = context;
            this.resource = resource;
            this.adapter = adapter;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final View view;
            if (convertView != null) {
                view = convertView;
            } else {
                final LayoutInflater inflater = LayoutInflater.from(context);
                view = inflater.inflate(resource, parent, false);
                final GroupViewHolder holder = new GroupViewHolder(view);
                view.setTag(holder);
            }

            // Update the background based on the state of the selected client.
            final RemoteClient client = getItem(position);
            final boolean isSelected = client.guid.equals(sState.selectedClient);
            adapter.updateClientsItemView(isSelected, context, view, getItem(position));
            return view;
        }
    }

    /**
     * OnTouchListener implementation for ListView that enables swipe to refresh on the touch down event iff list cannot scroll up.
     * This implementation does not consume the <code>MotionEvent</code>.
     */
    private class ListTouchListener implements View.OnTouchListener {
        private final AbsListView listView;

        public ListTouchListener(AbsListView listView) {
            this.listView = listView;
        }

        @Override
        public boolean onTouch(View v, MotionEvent event) {
            final int action = event.getAction();
            switch (action) {
                case MotionEvent.ACTION_DOWN:
                    // Enable swipe to refresh iff the first item is visible and is at the top.
                    mRefreshLayout.setEnabled(listView.getCount() <= 0
                    	    || (listView.getFirstVisiblePosition() <= 0 && listView.getChildAt(0).getTop() >= 0));
                    break;
                case MotionEvent.ACTION_CANCEL:
                case MotionEvent.ACTION_UP:
                    mRefreshLayout.setEnabled(true);
                    break;
            }

            // Event is not handled here, it will be consumed in enclosing SwipeRefreshLayout.
            return false;
        }
    }
}
