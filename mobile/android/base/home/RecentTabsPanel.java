/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.CommonColumns;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.home.HomePager.OnNewTabsListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Fragment that displays tabs from last session in a ListView.
 */
public class RecentTabsPanel extends HomeFragment
                             implements NativeEventListener {
    // Logging tag name
    @SuppressWarnings("unused")
    private static final String LOGTAG = "GeckoRecentTabsPanel";

    // Cursor loader ID for the loader that loads recent tabs
    private static final int LOADER_ID_RECENT_TABS = 0;

    // Adapter for the list of recent tabs.
    private RecentTabsAdapter mAdapter;

    // The view shown by the fragment.
    private HomeListView mList;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // On new tabs listener
    private OnNewTabsListener mNewTabsListener;

    // Recently closed tabs from gecko
    private ClosedTab[] mClosedTabs;

    private static final class ClosedTab {
        public final String url;
        public final String title;

        public ClosedTab(String url, String title) {
            this.url = url;
            this.title = title;
        }
    }

    public static final class RecentTabs implements URLColumns, CommonColumns {
        public static final String TYPE = "type";

        public static final int TYPE_HEADER = 0;
        public static final int TYPE_LAST_TIME = 1;
        public static final int TYPE_CLOSED = 2;
        public static final int TYPE_OPEN_ALL_LAST_TIME = 3;
        public static final int TYPE_OPEN_ALL_CLOSED = 4;
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mNewTabsListener = (OnNewTabsListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnNewTabsListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mNewTabsListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_recent_tabs_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_RECENT_TABS);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                final int itemType = c.getInt(c.getColumnIndexOrThrow(RecentTabs.TYPE));

                if (itemType == RecentTabs.TYPE_OPEN_ALL_LAST_TIME) {
                    openTabsWithType(RecentTabs.TYPE_LAST_TIME);
                    return;
                }

                if (itemType == RecentTabs.TYPE_OPEN_ALL_CLOSED) {
                    openTabsWithType(RecentTabs.TYPE_CLOSED);
                    return;
                }

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM);

                final ArrayList<String> urls = new ArrayList<String>();
                urls.add(c.getString(c.getColumnIndexOrThrow(RecentTabs.URL)));

                mNewTabsListener.onNewTabs(urls);
            }
        });

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                // Don't show context menus for the "Open all" rows.
                final int itemType = cursor.getInt(cursor.getColumnIndexOrThrow(RecentTabs.TYPE));
                if (itemType == RecentTabs.TYPE_OPEN_ALL_LAST_TIME || itemType == RecentTabs.TYPE_OPEN_ALL_CLOSED) {
                    return null;
                }

                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(RecentTabs.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(RecentTabs.TITLE));
                return info;
            }
        });

        registerForContextMenu(mList);

        EventDispatcher.getInstance().registerGeckoThreadListener(this, "ClosedTabs:Data");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("ClosedTabs:StartNotifications", null));
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mEmptyView = null;

        EventDispatcher.getInstance().unregisterGeckoThreadListener(this, "ClosedTabs:Data");
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("ClosedTabs:StopNotifications", null));
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize adapter
        mAdapter = new RecentTabsAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            return;
        }

        if (mEmptyView == null) {
            // Set empty panel view. We delay this so that the empty view won't flash.
            final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_last_tabs_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_last_tabs_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_RECENT_TABS, null, mCursorLoaderCallbacks);
    }

    @Override
    public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
        final NativeJSObject[] tabs = message.getObjectArray("tabs");
        final int length = tabs.length;

        final ClosedTab[] closedTabs = new ClosedTab[length];
        for (int i = 0; i < length; i++) {
            final NativeJSObject tab = tabs[i];
            closedTabs[i] = new ClosedTab(tab.getString("url"), tab.getString("title"));
        }

        // Only modify mClosedTabs on the UI thread
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                mClosedTabs = closedTabs;

                // The fragment might have been detached before this code
                // runs in the UI thread.
                if (getActivity() != null) {
                    // Reload the cursor to show recently closed tabs.
                    getLoaderManager().restartLoader(LOADER_ID_RECENT_TABS, null, mCursorLoaderCallbacks);
                }
            }
        });
    }

    private void openTabsWithType(int type) {
        final Cursor c = mAdapter.getCursor();
        if (c == null || !c.moveToFirst()) {
            return;
        }

        final List<String> urls = new ArrayList<String>();
        do {
            if (c.getInt(c.getColumnIndexOrThrow(RecentTabs.TYPE)) == type) {
                urls.add(c.getString(c.getColumnIndexOrThrow(RecentTabs.URL)));
            }
        } while (c.moveToNext());

        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.BUTTON);

        mNewTabsListener.onNewTabs(urls);
    }

    private static class RecentTabsCursorLoader extends SimpleCursorLoader {
        private final ClosedTab[] closedTabs;

        public RecentTabsCursorLoader(Context context, ClosedTab[] closedTabs) {
            super(context);
            this.closedTabs = closedTabs;
        }

        private void addRow(MatrixCursor c, String url, String title, int type) {
            final RowBuilder row = c.newRow();
            row.add(-1);
            row.add(url);
            row.add(title);
            row.add(type);
        }

        @Override
        public Cursor loadCursor() {
            final Context context = getContext();

            final MatrixCursor c = new MatrixCursor(new String[] { RecentTabs._ID,
                                                                   RecentTabs.URL,
                                                                   RecentTabs.TITLE,
                                                                   RecentTabs.TYPE });

            if (closedTabs != null && closedTabs.length > 0) {
                // How many closed tabs are actually displayed.
                int visibleClosedTabs = 0;

                final int length = closedTabs.length;
                for (int i = 0; i < length; i++) {
                    final String url = closedTabs[i].url;

                    // Don't show recent tabs for about:home or about:privatebrowsing.
                    if (!AboutPages.isTitlelessAboutPage(url)) {
                        // If this is the first closed tab we're adding, add a header for the section.
                        if (visibleClosedTabs == 0) {
                            addRow(c, null, context.getString(R.string.home_closed_tabs_title), RecentTabs.TYPE_HEADER);
                        }
                        addRow(c, url, closedTabs[i].title, RecentTabs.TYPE_CLOSED);
                        visibleClosedTabs++;
                    }
                }

                // Add an "Open all" button if more than 2 tabs were added to the list.
                if (visibleClosedTabs > 1) {
                    addRow(c, null, null, RecentTabs.TYPE_OPEN_ALL_CLOSED);
                }
            }

            final String jsonString = GeckoProfile.get(context).readSessionFile(true);
            if (jsonString == null) {
                // No previous session data
                return c;
            }

            final int count = c.getCount();

            new SessionParser() {
                @Override
                public void onTabRead(SessionTab tab) {
                    final String url = tab.getUrl();

                    // Don't show last tabs for about:home
                    if (AboutPages.isAboutHome(url)) {
                        return;
                    }

                    // If this is the first tab we're reading, add a header.
                    if (c.getCount() == count) {
                        addRow(c, null, context.getString(R.string.home_last_tabs_title), RecentTabs.TYPE_HEADER);
                    }

                    addRow(c, url, tab.getTitle(), RecentTabs.TYPE_LAST_TIME);
                }
            }.parse(jsonString);

            // Add an "Open all" button if more than 2 tabs were added to the list (account for the header)
            if (c.getCount() - count > 2) {
                addRow(c, null, null, RecentTabs.TYPE_OPEN_ALL_LAST_TIME);
            }

            return c;
        }
    }

    private static class RecentTabsAdapter extends MultiTypeCursorAdapter {
        private static final int ROW_HEADER = 0;
        private static final int ROW_STANDARD = 1;
        private static final int ROW_OPEN_ALL = 2;

        private static final int[] VIEW_TYPES = new int[] { ROW_STANDARD, ROW_HEADER, ROW_OPEN_ALL };
        private static final int[] LAYOUT_TYPES =
            new int[] { R.layout.home_item_row, R.layout.home_header_row, R.layout.home_open_all_row };

        public RecentTabsAdapter(Context context) {
            super(context, null, VIEW_TYPES, LAYOUT_TYPES);
        }

        public int getItemViewType(int position) {
            final Cursor c = getCursor(position);
            final int type = c.getInt(c.getColumnIndexOrThrow(RecentTabs.TYPE));

            if (type == RecentTabs.TYPE_HEADER) {
                return ROW_HEADER;
            }

            if (type == RecentTabs.TYPE_OPEN_ALL_LAST_TIME || type == RecentTabs.TYPE_OPEN_ALL_CLOSED) {
                return ROW_OPEN_ALL;
            }

            return ROW_STANDARD;
         }

        public boolean isEnabled(int position) {
            return (getItemViewType(position) != ROW_HEADER);
        }

        @Override
        public void bindView(View view, Context context, int position) {
            final int itemType = getItemViewType(position);
            if (itemType == ROW_OPEN_ALL) {
                return;
            }

            final Cursor c = getCursor(position);

            if (itemType == ROW_HEADER) {
                final String title = c.getString(c.getColumnIndexOrThrow(RecentTabs.TITLE));
                final TextView textView = (TextView) view;
                textView.setText(title);
            } else if (itemType == ROW_STANDARD) {
                final TwoLinePageRow pageRow = (TwoLinePageRow) view;
                pageRow.updateFromCursor(c);
            }
         }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new RecentTabsCursorLoader(getActivity(), mClosedTabs);
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            mAdapter.swapCursor(c);
            updateUiFromCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mAdapter.swapCursor(null);
        }
    }
}
