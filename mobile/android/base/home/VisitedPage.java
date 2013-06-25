/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.SimpleCursorLoader;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class VisitedPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoVisitedPage";

    // Cursor loader ID for search query
    private static final int FRECENCY_LOADER_ID = 0;

    // Cursor loader ID for favicons query
    private static final int FAVICONS_LOADER_ID = 1;

    // Adapter for the list of search results
    private VisitedAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Inflater used by the adapter
    private LayoutInflater mInflater;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public VisitedPage() {
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

        mInflater = (LayoutInflater) activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mInflater = null;
        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        mList = new HomeListView(container.getContext());
        return mList;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

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
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize the search adapter
        mAdapter = new VisitedAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();

        // Reconnect to the loader only if present
        getLoaderManager().initLoader(FRECENCY_LOADER_ID, null, mCursorLoaderCallbacks);
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

    private class VisitedAdapter extends SimpleCursorAdapter {
        public VisitedAdapter(Context context) {
            super(context, -1, null, new String[] {}, new int[] {});
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final TwoLinePageRow row;
            if (convertView == null) {
                row = (TwoLinePageRow) mInflater.inflate(R.layout.home_item_row, mList, false);
            } else {
                row = (TwoLinePageRow) convertView;
            }

            final Cursor c = getCursor();
            if (!c.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            row.updateFromCursor(c);

            return row;
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            switch(id) {
            case FRECENCY_LOADER_ID:
                return new FrecencyCursorLoader(getActivity());

            case FAVICONS_LOADER_ID:
                return FaviconsLoader.createInstance(getActivity(), args);
            }

            return null;
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case FRECENCY_LOADER_ID:
                mAdapter.swapCursor(c);

                FaviconsLoader.restartFromCursor(getLoaderManager(), FAVICONS_LOADER_ID,
                        mCursorLoaderCallbacks, c);
                break;

            case FAVICONS_LOADER_ID:
                // Causes the listview to recreate its children and use the
                // now in-memory favicons.
                mAdapter.notifyDataSetChanged();
                break;
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case FRECENCY_LOADER_ID:
                mAdapter.swapCursor(null);
                break;

            case FAVICONS_LOADER_ID:
                // Do nothing
                break;
            }
        }
    }
}