/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB.URLColumns;

import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.DialogFragment;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.ListView;

/**
 * Dialog fragment that displays frecency search results, for pinning as a bookmark, in a ListView.
 */
class PinBookmarkDialog extends DialogFragment {
    // Listener for url selection
    public static interface OnBookmarkSelectedListener {
        public void onBookmarkSelected(String url, String title);
    }

    // Cursor loader ID for search query
    private static final int LOADER_ID_SEARCH = 0;

    // Cursor loader ID for favicons query
    private static final int LOADER_ID_FAVICONS = 1;

    // Holds the current search term to use in the query
    private String mSearchTerm;

    // Adapter for the list of search results
    private SearchAdapter mAdapter;

    // Search entry
    private EditText mSearch;

    // Search results
    private ListView mList;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mLoaderCallbacks;

    // Bookmark selected listener
    private OnBookmarkSelectedListener mOnBookmarkSelectedListener;

    public static PinBookmarkDialog newInstance() {
        return new PinBookmarkDialog();
    }

    private PinBookmarkDialog() {
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setStyle(DialogFragment.STYLE_NO_TITLE, android.R.style.Theme_Holo_Light_Dialog);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        // All list views are styled to look the same with a global activity theme.
        // If the style of the list changes, inflate it from an XML.
        return inflater.inflate(R.layout.pin_bookmark_dialog, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mSearch = (EditText) view.findViewById(R.id.search);
        mSearch.addTextChangedListener(new TextWatcher() {
            @Override
            public void afterTextChanged(Editable s) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                filter(mSearch.getText().toString());
            }
        });

        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (mOnBookmarkSelectedListener != null) {
                    final Cursor c = mAdapter.getCursor();
                    if (c == null || !c.moveToPosition(position)) {
                        return;
                    }

                    final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));
                    final String title = c.getString(c.getColumnIndexOrThrow(URLColumns.TITLE));
                    mOnBookmarkSelectedListener.onBookmarkSelected(url, title);
                }

                // Dismiss the fragment and the dialog.
                dismiss();
            }
        });
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Initialize the search adapter
        mAdapter = new SearchAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mLoaderCallbacks = new CursorLoaderCallbacks();

        // Reconnect to the loader only if present
        getLoaderManager().initLoader(LOADER_ID_SEARCH, null, mLoaderCallbacks);

        // Default filter.
        filter("");
    }

    private void filter(String searchTerm) {
        if (!TextUtils.isEmpty(searchTerm) &&
            TextUtils.equals(mSearchTerm, searchTerm)) {
            return;
        }

        mSearchTerm = searchTerm;

        // Restart loaders with the new search term
        SearchLoader.restart(getLoaderManager(), LOADER_ID_SEARCH, mLoaderCallbacks, mSearchTerm);
    }

    public void setOnBookmarkSelectedListener(OnBookmarkSelectedListener listener) {
        mOnBookmarkSelectedListener = listener;
    }

    private static class SearchAdapter extends CursorAdapter {
        private LayoutInflater mInflater;

        public SearchAdapter(Context context) {
            super(context, null);
            mInflater = LayoutInflater.from(context);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            TwoLinePageRow row = (TwoLinePageRow) view;
            row.setShowIcons(false);
            row.updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return (TwoLinePageRow) mInflater.inflate(R.layout.home_item_row, parent, false);
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            switch(id) {
            case LOADER_ID_SEARCH:
                return SearchLoader.createInstance(getActivity(), args);

            case LOADER_ID_FAVICONS:
                return FaviconsLoader.createInstance(getActivity(), args);
            }

            return null;
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case LOADER_ID_SEARCH:
                mAdapter.swapCursor(c);

                FaviconsLoader.restartFromCursor(getLoaderManager(), LOADER_ID_FAVICONS,
                        mLoaderCallbacks, c);
                break;

            case LOADER_ID_FAVICONS:
                // Force the list to use the in-memory favicons.
                mAdapter.notifyDataSetChanged();
                break;
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            final int loaderId = loader.getId();
            switch(loaderId) {
            case LOADER_ID_SEARCH:
                mAdapter.swapCursor(null);
                break;

            case LOADER_ID_FAVICONS:
                // Do nothing
                break;
            }
        }
    }
}
