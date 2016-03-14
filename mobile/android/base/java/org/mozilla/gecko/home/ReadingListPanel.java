/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.reader.ReaderModeUtils;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.ReadingListItems;
import org.mozilla.gecko.db.BrowserContract.URLColumns;
import org.mozilla.gecko.db.ReadingListAccessor;
import org.mozilla.gecko.home.HomeContextMenuInfo.RemoveItemType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.Loader;
import android.support.v4.app.LoaderManager;
import android.support.v4.widget.CursorAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.gecko.util.NetworkUtils;

/**
 * Fragment that displays reading list contents in a ListView.
 */
public class ReadingListPanel extends HomeFragment {

    // Cursor loader ID for reading list
    private static final int LOADER_ID_READING_LIST = 0;

    // Formatted string in hint text to be replaced with an icon.
    private final String MATCH_STRING = "%I";

    // Adapter for the list of reading list items
    private ReadingListAdapter mAdapter;

    // The view shown by the fragment
    private HomeListView mList;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Reference to top view.
    private View mTopView;

    // Callbacks used for the reading list and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_list_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mTopView = view;

        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_READING_LIST);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final Context context = getActivity();
                if (context == null) {
                    return;
                }

                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
                url = ReaderModeUtils.getAboutReaderForUrl(url);

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM, "reading_list");
                Telemetry.addToHistogram("FENNEC_LOAD_SAVED_PAGE", NetworkUtils.isConnected(context) ? 0 : 1);

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));

                markAsRead(context, id);
            }
        });

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(ReadingListItems.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(ReadingListItems.TITLE));
                info.readingListItemId = cursor.getInt(cursor.getColumnIndexOrThrow(ReadingListItems._ID));
                info.isUnread = cursor.getInt(cursor.getColumnIndexOrThrow(ReadingListItems.IS_UNREAD)) == 1;
                info.itemType = RemoveItemType.READING_LIST;
                return info;
            }
        });
        registerForContextMenu(mList);
    }

    private void markAsRead(final Context context, final long id) {
        GeckoProfile.get(context).getDB().getReadingListAccessor().markAsRead(
            context.getContentResolver(),
            id
        );
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        // Discard any additional item clicks on the list as the
        // panel is getting destroyed (bug 1210243).
        mList.setOnItemClickListener(null);

        mList = null;
        mTopView = null;
        mEmptyView = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mAdapter = new ReadingListAdapter(getActivity(), null);
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started.
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_READING_LIST, null, mCursorLoaderCallbacks);
    }

    private void updateUiFromCursor(Cursor c) {
        // We delay setting the empty view until the cursor is actually empty.
        // This avoids image flashing.
        if ((c == null || c.getCount() == 0) && mEmptyView == null) {
            final ViewStub emptyViewStub = (ViewStub) mTopView.findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_reading_list_empty);

            final ImageView emptyImage = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyImage.setImageResource(R.drawable.icon_reading_list_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    /**
     * Cursor loader for the list of reading list items.
     */
    private static class ReadingListLoader extends SimpleCursorLoader {
        private final ReadingListAccessor accessor;

        public ReadingListLoader(Context context) {
            super(context);
            accessor = GeckoProfile.get(context).getDB().getReadingListAccessor();
        }

        @Override
        public Cursor loadCursor() {
            return accessor.getReadingList(getContext().getContentResolver());
        }
    }

    /**
     * Cursor adapter for the list of reading list items.
     */
    private class ReadingListAdapter extends CursorAdapter {
        public ReadingListAdapter(Context context, Cursor cursor) {
            super(context, cursor, 0);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            final ReadingListRow row = (ReadingListRow) view;
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(parent.getContext()).inflate(R.layout.reading_list_item_row, parent, false);
        }
    }

    /**
     * LoaderCallbacks implementation that interacts with the LoaderManager.
     */
    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new ReadingListLoader(getActivity());
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
