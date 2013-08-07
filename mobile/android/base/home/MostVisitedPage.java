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
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

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

    // Empty message view
    private View mEmptyMessage;

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
        final TextView title = (TextView) view.findViewById(R.id.title);
        title.setText(R.string.home_most_visited_title);

        mEmptyMessage = view.findViewById(R.id.empty_message);
        mList = (HomeListView) view.findViewById(R.id.list);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
                mUrlOpenListener.onUrlOpen(url);
            }
        });

        registerForContextMenu(mList);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mEmptyMessage = null;
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
                // Only set empty view once cursor is loaded to avoid
                // flashing the empty message before loading.
                mList.setEmptyView(mEmptyMessage);

                mAdapter.swapCursor(c);
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
