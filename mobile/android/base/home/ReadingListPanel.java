/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.EnumSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.ReaderModeUtils;
import org.mozilla.gecko.db.BrowserContract.Bookmarks;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ImageSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.TextView;

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

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement HomePager.OnUrlOpenListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_reading_list_panel, container, false);
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
                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
                url = ReaderModeUtils.getAboutReaderForUrl(url);

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mList.setContextMenuInfoFactory(new HomeListView.ContextMenuInfoFactory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(URLColumns.TITLE));
                info.bookmarkId = cursor.getInt(cursor.getColumnIndexOrThrow(Bookmarks._ID));
                info.inReadingList = true;
                return info;
            }
        });
        registerForContextMenu(mList);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mTopView = null;
        mEmptyView = null;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // Detach and reattach the fragment as the layout changes.
        if (isVisible()) {
            getFragmentManager().beginTransaction()
                                .detach(this)
                                .attach(this)
                                .commitAllowingStateLoss();
        }
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

            final TextView emptyHint = (TextView) mEmptyView.findViewById(R.id.home_empty_hint);
            String readingListHint = emptyHint.getText().toString();

            // Use an ImageSpan to include the reader icon in the "Tip".
            int imageSpanIndex = readingListHint.indexOf(MATCH_STRING);
            if (imageSpanIndex != -1) {
                final ImageSpan readingListIcon = new ImageSpan(getActivity(), R.drawable.reader_cropped, ImageSpan.ALIGN_BOTTOM);
                final SpannableStringBuilder hintBuilder = new SpannableStringBuilder(readingListHint);

                // Add additional spacing.
                hintBuilder.insert(imageSpanIndex + MATCH_STRING.length(), " ");
                hintBuilder.insert(imageSpanIndex, " ");

                // Add icon.
                hintBuilder.setSpan(readingListIcon, imageSpanIndex + 1, imageSpanIndex + MATCH_STRING.length() + 1, Spanned.SPAN_INCLUSIVE_INCLUSIVE);

                emptyHint.setText(hintBuilder, TextView.BufferType.SPANNABLE);
            }

            mList.setEmptyView(mEmptyView);
        }
    }

    /**
     * Cursor loader for the list of reading list items.
     */
    private static class ReadingListLoader extends SimpleCursorLoader {
        public ReadingListLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            return BrowserDB.getBookmarksInFolder(getContext().getContentResolver(), Bookmarks.FIXED_READING_LIST_ID);
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
            final TwoLinePageRow row = (TwoLinePageRow) view;
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(parent.getContext()).inflate(R.layout.bookmark_item_row, parent, false);
        }
    }

    /**
     * LoaderCallbacks implementation that interacts with the LoaderManager.
     */
    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
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
