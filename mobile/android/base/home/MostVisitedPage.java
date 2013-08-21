/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import java.util.EnumSet;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class MostVisitedPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoMostVisitedPage";

    // Cursor loader ID for search query
    private static final int LOADER_ID_FRECENCY = 0;

    // Adapter for the list of search results
    private VisitedAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // The title for this HomeFragment page.
    private TextView mTitle;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public static MostVisitedPage newInstance() {
        return new MostVisitedPage();
    }

    public MostVisitedPage() {
        mUrlOpenListener = null;
    }

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
        return inflater.inflate(R.layout.home_most_visited_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mTitle = (TextView) view.findViewById(R.id.title);
        if (mTitle != null) {
            mTitle.setText(R.string.home_most_visited_title);
        }

        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_MOST_VISITED);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        registerForContextMenu(mList);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mTitle = null;
        mEmptyView = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final Activity activity = getActivity();

        // Intialize the search adapter
        mAdapter = new VisitedAdapter(activity, null);
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks(activity, getLoaderManager());
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_FRECENCY, null, mCursorLoaderCallbacks);
    }

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            if (mTitle != null) {
                mTitle.setVisibility(View.VISIBLE);
            }
            return;
        }

        // Cursor is empty, so hide the title and set the
        // empty view if it hasn't been set already.
        if (mTitle != null) {
            mTitle.setVisibility(View.GONE);
        }

        if (mEmptyView == null) {
            // Set empty page view. We delay this so that the empty view won't flash.
            ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_most_visited_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_most_visited_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    private static class FrecencyCursorLoader extends SimpleCursorLoader {
        // Max number of search results
        private static final int SEARCH_LIMIT = 50;

        public FrecencyCursorLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();
            return BrowserDB.filter(cr, "", SEARCH_LIMIT);
        }
    }

    private class VisitedAdapter extends CursorAdapter {
        public VisitedAdapter(Context context, Cursor cursor) {
            super(context, cursor);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            final TwoLinePageRow row = (TwoLinePageRow) view;
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(parent.getContext()).inflate(R.layout.home_item_row, parent, false);
        }
    }

    private class CursorLoaderCallbacks extends HomeCursorLoaderCallbacks {
        public CursorLoaderCallbacks(Context context, LoaderManager loaderManager) {
            super(context, loaderManager);
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (id == LOADER_ID_FRECENCY) {
                return new FrecencyCursorLoader(getActivity());
            } else {
                return super.onCreateLoader(id, args);
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            if (loader.getId() == LOADER_ID_FRECENCY) {
                mAdapter.swapCursor(c);
                updateUiFromCursor(c);
                loadFavicons(c);
            } else {
                super.onLoadFinished(loader, c);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (loader.getId() == LOADER_ID_FRECENCY) {
                mAdapter.swapCursor(null);
            } else {
                super.onLoaderReset(loader);
            }
        }

        @Override
        public void onFaviconsLoaded() {
            mAdapter.notifyDataSetChanged();
        }
    }
}
