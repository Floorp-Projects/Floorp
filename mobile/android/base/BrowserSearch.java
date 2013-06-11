/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.content.res.Configuration;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.SimpleCursorAdapter;

import java.util.ArrayList;

/**
 * Fragment that displays frecency search results in a ListView.
 */
public class BrowserSearch extends Fragment implements AdapterView.OnItemClickListener {
    // Cursor loader ID for search query
    private static final int SEARCH_LOADER_ID = 0;

    // Cursor loader ID for favicons query
    private static final int FAVICONS_LOADER_ID = 1;

    // Argument containing list of urls for the favicons loader
    private static final String FAVICONS_LOADER_URLS_ARG = "urls";

    // Holds the current search term to use in the query
    private String mSearchTerm;

    // Adapter for the list of search results
    private SearchAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public interface OnUrlOpenListener {
        public void onUrlOpen(String url);
    }

    public static BrowserSearch newInstance() {
        return new BrowserSearch();
    }

    public BrowserSearch() {
        mSearchTerm = "";
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        try {
            mUrlOpenListener = (OnUrlOpenListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement BrowserSearch.OnUrlOpenListener");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        mList = new ListView(container.getContext());
        return mList;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mList.setOnItemClickListener(this);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize the search adapter
        mAdapter = new SearchAdapter(getActivity());
        mList.setAdapter(mAdapter);

        mCursorLoaderCallbacks = new CursorLoaderCallbacks();

        // Reconnect to the loader only if present
        getLoaderManager().initLoader(SEARCH_LOADER_ID, null, mCursorLoaderCallbacks);
    }

    private ArrayList<String> getUrlsWithoutFavicon(Cursor c) {
        ArrayList<String> urls = new ArrayList<String>();

        if (c == null || !c.moveToFirst()) {
            return urls;
        }

        final Favicons favicons = Favicons.getInstance();

        do {
            final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

            // We only want to load favicons from DB if they are not in the
            // memory cache yet.
            if (favicons.getFaviconFromMemCache(url) != null) {
                continue;
            }

            urls.add(url);
        } while (c.moveToNext());

        return urls;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        final Cursor c = mAdapter.getCursor();
        if (c == null || !c.moveToPosition(position)) {
            return;
        }

        final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
        mUrlOpenListener.onUrlOpen(url);
    }

    public void filter(String searchTerm) {
        if (TextUtils.isEmpty(searchTerm)) {
            return;
        }

        if (TextUtils.equals(mSearchTerm, searchTerm)) {
            return;
        }

        mSearchTerm = searchTerm;

        if (isVisible()) {
            getLoaderManager().restartLoader(SEARCH_LOADER_ID, null, mCursorLoaderCallbacks);

        }
    }

    private static class SearchCursorLoader extends SimpleCursorLoader {
        // Max number of search results
        private static final int SEARCH_LIMIT = 100;

        // The target search term associated with the loader
        private final String mSearchTerm;

        public SearchCursorLoader(Context context, String searchTerm) {
            super(context);
            mSearchTerm = searchTerm;
        }

        @Override
        public Cursor loadCursor() {
            if (TextUtils.isEmpty(mSearchTerm)) {
                return null;
            }

            final ContentResolver cr = getContext().getContentResolver();
            return BrowserDB.filter(cr, mSearchTerm, SEARCH_LIMIT);
        }
    }

    private static class FaviconsCursorLoader extends SimpleCursorLoader {
        private ArrayList<String> mUrls;

        public FaviconsCursorLoader(Context context, ArrayList<String> urls) {
            super(context);
            mUrls = urls;
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();

            Cursor c = BrowserDB.getFaviconsForUrls(cr, mUrls);
            storeFaviconsInMemCache(c);

            return c;
        }

        private void storeFaviconsInMemCache(Cursor c) {
            if (c == null || !c.moveToFirst()) {
                return;
            }

            final Favicons favicons = Favicons.getInstance();

            do {
                final String url = c.getString(c.getColumnIndexOrThrow(Combined.URL));
                final byte[] b = c.getBlob(c.getColumnIndexOrThrow(Combined.FAVICON));

                if (b == null || b.length == 0) {
                    continue;
                }

                Bitmap favicon = BitmapUtils.decodeByteArray(b);
                if (favicon == null) {
                    continue;
                }

                favicon = favicons.scaleImage(favicon);
                favicons.putFaviconInMemCache(url, favicon);
            } while (c.moveToNext());
        }
    }

    private class SearchAdapter extends SimpleCursorAdapter {
        public SearchAdapter(Context context) {
            super(context, -1, null, new String[] {}, new int[] {});
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final TwoLinePageRow row;
            if (convertView == null) {
                row = (TwoLinePageRow) LayoutInflater.from(getActivity()).inflate(R.layout.home_item_row, null);
            } else {
                row = (TwoLinePageRow) convertView;
            }

            final Cursor c = getCursor();
            if (!c.moveToPosition(position)) {
                throw new IllegalStateException("Couldn't move cursor to position " + position);
            }

            row.updateFromCursor(c);

            // FIXME: show bookmark icon

            return row;
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            switch(id) {
            case SEARCH_LOADER_ID:
                return new SearchCursorLoader(getActivity(), mSearchTerm);

            case FAVICONS_LOADER_ID:
                final ArrayList<String> urls = args.getStringArrayList(FAVICONS_LOADER_URLS_ARG);
                return new FaviconsCursorLoader(getActivity(), urls);
            }

            return null;
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case SEARCH_LOADER_ID:
                mAdapter.swapCursor(c);

                // If there urls without in-memory favicons, trigger a new loader
                // to load the images from disk to memory.
                ArrayList<String> urls = getUrlsWithoutFavicon(c);
                if (urls.size() > 0) {
                    Bundle args = new Bundle();
                    args.putStringArrayList(FAVICONS_LOADER_URLS_ARG, urls);
                    getLoaderManager().restartLoader(FAVICONS_LOADER_ID, args, mCursorLoaderCallbacks);
                }
                break;

            case FAVICONS_LOADER_ID:
                // Causes the listview to recreate its children and use the
                // now in-memory favicons.
                mList.requestLayout();
                break;
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case SEARCH_LOADER_ID:
                mAdapter.swapCursor(null);
                break;

            case FAVICONS_LOADER_ID:
                // Do nothing
                break;
            }
        }
    }
}