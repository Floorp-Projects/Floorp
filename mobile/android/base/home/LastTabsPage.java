/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.SessionParser;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.home.HomePager.OnNewTabsListener;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.database.MatrixCursor.RowBuilder;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.LoaderManager;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.support.v4.widget.SimpleCursorAdapter;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.view.LayoutInflater;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.util.Date;

/**
 * Fragment that displays tabs from last session in a ListView.
 */
public class LastTabsPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoLastTabsPage";

    // Cursor loader ID for the session parser
    private static final int LAST_TABS_LOADER_ID = 0;

    // Adapter for the list of search results
    private LastTabsAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Inflater used by the adapter
    private LayoutInflater mInflater;

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

        mInflater = (LayoutInflater) activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    }

    @Override
    public void onDetach() {
        super.onDetach();

        mInflater = null;
        mNewTabsListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_last_tabs_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        final TextView title = (TextView) view.findViewById(R.id.title);
        title.setText(R.string.home_last_tabs_title);

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

        final View openAllTabsButton = view.findViewById(R.id.open_all_tabs_button);
        openAllTabsButton.setOnClickListener(new View.OnClickListener() {
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
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize adapter
        mAdapter = new LastTabsAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();

        // Reconnect to the loader only if present
        getLoaderManager().initLoader(LAST_TABS_LOADER_ID, null, mCursorLoaderCallbacks);
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

    private class LastTabsAdapter extends SimpleCursorAdapter {
        public LastTabsAdapter(Context context) {
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
            return new LastTabsCursorLoader(getActivity());
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            mAdapter.swapCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mAdapter.swapCursor(null);
        }
    }
}
