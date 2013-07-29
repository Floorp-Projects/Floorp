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
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ListView;

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

    // Empty message view
    private View mEmptyMessage;

    // Buttons container
    private View mButtonsContainer;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

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
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_visited_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mList = (HomeListView) view.findViewById(R.id.visited_list);

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

        mEmptyMessage = view.findViewById(R.id.empty_message);
        mButtonsContainer = view.findViewById(R.id.buttons_container);

        final View historyButton = view.findViewById(R.id.history_button);
        historyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showHistoryPage();
            }
        });

        final View tabsButton = view.findViewById(R.id.tabs_button);
        tabsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showTabsPage();
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mButtonsContainer = null;
        mEmptyMessage = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize the search adapter
        mAdapter = new VisitedAdapter(getActivity(), null);
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(FRECENCY_LOADER_ID, null, mCursorLoaderCallbacks);
    }

    private void showHistoryPage() {
        final HistoryPage historyPage = HistoryPage.newInstance();
        showSubPage(historyPage);
    }

    private void showTabsPage() {
        final LastTabsPage lastTabsPage = LastTabsPage.newInstance();
        showSubPage(lastTabsPage);
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
                // Only set empty view once cursor is loaded to avoid
                // flashing the empty message before loading.
                mList.setEmptyView(mEmptyMessage);

                final int buttonsVisibility = (c.getCount() == 0 ? View.GONE : View.VISIBLE);
                mButtonsContainer.setVisibility(buttonsVisibility);

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