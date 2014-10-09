/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search;

import android.app.Activity;
import android.database.Cursor;
import android.graphics.Rect;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.CursorLoader;
import android.support.v4.content.Loader;
import android.support.v4.widget.SimpleCursorAdapter;
import android.text.TextUtils;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserContract.SearchHistory;
import org.mozilla.gecko.widget.SwipeDismissListViewTouchListener;
import org.mozilla.gecko.widget.SwipeDismissListViewTouchListener.OnDismissCallback;
import org.mozilla.search.AcceptsSearchQuery.SuggestionAnimation;

/**
 * This fragment is responsible for managing the card stream.
 */
public class PreSearchFragment extends Fragment {

    private static final String LOG_TAG = "PreSearchFragment";

    private AcceptsSearchQuery searchListener;
    private SimpleCursorAdapter cursorAdapter;

    private ListView listView;
    private View emptyView;

    private static final String[] PROJECTION = new String[]{ SearchHistory.QUERY, SearchHistory._ID };

    // Limit search history query results to 10 items.
    private static final int SEARCH_HISTORY_LIMIT = 10;

    private static final Uri SEARCH_HISTORY_URI = SearchHistory.CONTENT_URI.buildUpon().
            appendQueryParameter(BrowserContract.PARAM_LIMIT, String.valueOf(SEARCH_HISTORY_LIMIT)).build();

    private static final int LOADER_ID_SEARCH_HISTORY = 1;

    public PreSearchFragment() {
        // Mandatory empty constructor for Android's Fragment.
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);

        if (activity instanceof AcceptsSearchQuery) {
            searchListener = (AcceptsSearchQuery) activity;
        } else {
            throw new ClassCastException(activity.toString() + " must implement AcceptsSearchQuery.");
        }
    }

    @Override
    public void onDetach() {
        super.onDetach();
        searchListener = null;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getLoaderManager().initLoader(LOADER_ID_SEARCH_HISTORY, null, new SearchHistoryLoaderCallbacks());
        cursorAdapter = new SimpleCursorAdapter(getActivity(), R.layout.search_history_row, null,
                PROJECTION, new int[]{R.id.site_name}, 0);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        getLoaderManager().destroyLoader(LOADER_ID_SEARCH_HISTORY);
        cursorAdapter.swapCursor(null);
        cursorAdapter = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedState) {
        final View mainView = inflater.inflate(R.layout.search_fragment_pre_search, container, false);

        // Initialize listview.
        listView = (ListView) mainView.findViewById(R.id.list_view);
        listView.setAdapter(cursorAdapter);
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final String query = getQueryAtPosition(position);
                if (!TextUtils.isEmpty(query)) {
                    final Rect startBounds = new Rect();
                    view.getGlobalVisibleRect(startBounds);

                    Telemetry.sendUIEvent(TelemetryContract.Event.SEARCH, TelemetryContract.Method.HOMESCREEN, "history");

                    searchListener.onSearch(query, new SuggestionAnimation() {
                        @Override
                        public Rect getStartBounds() {
                            return startBounds;
                        }
                    });
                }
            }
        });

        // Create a ListView-specific touch listener. ListViews are given special treatment because
        // by default they handle touches for their list items... i.e. they're in charge of drawing
        // the pressed state (the list selector), handling list item clicks, etc.
        final SwipeDismissListViewTouchListener touchListener = new SwipeDismissListViewTouchListener(listView, new OnDismissCallback() {
            @Override
            public void onDismiss(ListView listView, final int position) {
                new AsyncTask<Void, Void, Void>() {
                    @Override
                    protected Void doInBackground(Void... params) {
                        final String query = getQueryAtPosition(position);
                        final int deleted = getActivity().getContentResolver().delete(
                                SearchHistory.CONTENT_URI,
                                SearchHistory.QUERY + " = ?",
                                new String[] { query });

                        if (deleted < 1) {
                            Log.w(LOG_TAG, "Search query not deleted: " + query);
                        }
                        return null;
                    }
                }.execute();
            }
        });
        listView.setOnTouchListener(touchListener);

        // Setting this scroll listener is required to ensure that during ListView scrolling,
        // we don't look for swipes.
        listView.setOnScrollListener(touchListener.makeScrollListener());

        // Setting this recycler listener is required to make sure animated views are reset.
        listView.setRecyclerListener(touchListener.makeRecyclerListener());

        return mainView;
    }

    private String getQueryAtPosition(int position) {
        final Cursor c = cursorAdapter.getCursor();
        if (c == null || !c.moveToPosition(position)) {
            return null;
        }
        return c.getString(c.getColumnIndexOrThrow(SearchHistory.QUERY));
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        listView.setAdapter(null);
        listView = null;
        emptyView = null;
    }

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            return;
        }

        if (emptyView == null) {
            final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.empty_view_stub);
            emptyView = emptyViewStub.inflate();

            ((ImageView) emptyView.findViewById(R.id.empty_image)).setImageResource(R.drawable.search_fox);
            ((TextView) emptyView.findViewById(R.id.empty_title)).setText(R.string.search_empty_title);
            ((TextView) emptyView.findViewById(R.id.empty_message)).setText(R.string.search_empty_message);

            listView.setEmptyView(emptyView);
        }
    }

    private class SearchHistoryLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new CursorLoader(getActivity(), SEARCH_HISTORY_URI, PROJECTION, null, null,
                    SearchHistory.DATE_LAST_VISITED + " DESC");
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            if (cursorAdapter != null) {
                cursorAdapter.swapCursor(c);
            }
            updateUiFromCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (cursorAdapter != null) {
                cursorAdapter.swapCursor(null);
            }
        }
    }
}
