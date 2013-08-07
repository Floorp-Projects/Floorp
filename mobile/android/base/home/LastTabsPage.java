/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.HomePager.OnNewTabsListener;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

/**
 * Fragment that displays tabs from last session in a ListView.
 */
public class LastTabsPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoLastTabsPage";

    // Cursor loader ID for the session parser
    private static final int LOADER_ID_LAST_TABS = 0;

    // Adapter for the list of search results
    private LastTabsAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // The title for this HomeFragment page.
    private TextView mTitle;

    // The button view for restoring tabs from last session.
    private View mRestoreButton;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // On new tabs listener
    private OnNewTabsListener mNewTabsListener;

    public static LastTabsPage newInstance() {
        return new LastTabsPage();
    }

    public LastTabsPage() {
        mNewTabsListener = null;
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
        return inflater.inflate(R.layout.home_last_tabs_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mTitle = (TextView) view.findViewById(R.id.title);
        if (mTitle != null) {
            mTitle.setText(R.string.home_last_tabs_title);
        }

        mList = (ListView) view.findViewById(R.id.list);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final Cursor c = mAdapter.getCursor();
                if (c == null || !c.moveToPosition(position)) {
                    return;
                }

                final String url = c.getString(c.getColumnIndexOrThrow(Combined.URL));
                mNewTabsListener.onNewTabs(new String[] { url });
            }
        });

        registerForContextMenu(mList);

        mRestoreButton = view.findViewById(R.id.open_all_tabs_button);
        mRestoreButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openAllTabs();
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mTitle = null;
        mEmptyView = null;
        mRestoreButton = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize adapter
        mAdapter = new LastTabsAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            if (mTitle != null) {
                mTitle.setVisibility(View.VISIBLE);
            }
            mRestoreButton.setVisibility(View.VISIBLE);
            return;
        }

        // Cursor is empty, so hide the title and set the empty view if it hasn't been set already.
        if (mTitle != null) {
            mTitle.setVisibility(View.VISIBLE);
        }
        mRestoreButton.setVisibility(View.GONE);

        if (mEmptyView == null) {
            // Set empty page view. We delay this so that the empty view won't flash.
            ViewStub emptyViewStub = (ViewStub) getActivity().findViewById(R.id.home_empty_view_stub);
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
        getLoaderManager().initLoader(LOADER_ID_LAST_TABS, null, mCursorLoaderCallbacks);
    }

    private void openAllTabs() {
        final Cursor c = mAdapter.getCursor();
        if (c == null || !c.moveToFirst()) {
            return;
        }

        final String[] urls = new String[c.getCount()];

        do {
            urls[c.getPosition()] = c.getString(c.getColumnIndexOrThrow(Combined.URL));
        } while (c.moveToNext());

        mNewTabsListener.onNewTabs(urls);
    }

    private static class LastTabsCursorLoader extends SimpleCursorLoader {
        public LastTabsCursorLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            final Context context = getContext();

            final String jsonString = GeckoProfile.get(context).readSessionFile(true);
            if (jsonString == null) {
                // No previous session data
                return null;
            }

            final MatrixCursor c = new MatrixCursor(new String[] { Combined._ID,
                                                                   Combined.URL,
                                                                   Combined.TITLE,
                                                                   Combined.FAVICON });

            new SessionParser() {
                @Override
                public void onTabRead(SessionTab tab) {
                    final String url = tab.getUrl();

                    // Don't show last tabs for about:home
                    if (url.equals("about:home")) {
                        return;
                    }

                    final RowBuilder row = c.newRow();
                    row.add(-1);
                    row.add(url);

                    final String title = tab.getTitle();
                    row.add(title);

                    final ContentResolver cr = context.getContentResolver();
                    final byte[] favicon = BrowserDB.getFaviconBytesForUrl(cr, url);
                    row.add(favicon);
                }
            }.parse(jsonString);

            return c;
        }
    }

    private static class LastTabsAdapter extends CursorAdapter {
        public LastTabsAdapter(Context context) {
            super(context, null);
        }

        @Override
        public void bindView(View view, Context context, Cursor cursor) {
            ((TwoLinePageRow) view).updateFromCursor(cursor);
        }

        @Override
        public View newView(Context context, Cursor cursor, ViewGroup parent) {
            return LayoutInflater.from(context).inflate(R.layout.home_item_row, parent, false);
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new LastTabsCursorLoader(getActivity());
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
