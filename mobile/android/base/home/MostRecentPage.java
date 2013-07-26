/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.home.TwoLinePageRow;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Bundle;
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

import java.util.Date;

/**
 * Fragment that displays recent history in a ListView.
 */
public class MostRecentPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoMostRecentPage";

    // Cursor loader ID for history query
    private static final int HISTORY_LOADER_ID = 0;

    // For the time sections in history
    private static final long MS_PER_DAY = 86400000;
    private static final long MS_PER_WEEK = MS_PER_DAY * 7;

    // The time ranges for each section
    private static enum MostRecentSection {
        TODAY,
        YESTERDAY,
        WEEK,
        OLDER
    };

    // Maps headers in the list with their respective sections
    private SparseArray<MostRecentSection> mMostRecentSections;

    // Adapter for the list of search results
    private MostRecentAdapter mAdapter;

    // The view shown by the fragment.
    private ListView mList;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // Inflater used by the adapter
    private LayoutInflater mInflater;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

    public static MostRecentPage newInstance() {
        return new MostRecentPage();
    }

    public MostRecentPage() {
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

        mMostRecentSections = null;
        mInflater = null;
        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_most_recent_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        final TextView title = (TextView) view.findViewById(R.id.title);
        title.setText(R.string.home_most_recent_title);

        mList = (ListView) view.findViewById(R.id.list);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                position -= getMostRecentSectionsCountBefore(position);

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

        // Initialize map of history sections
        mMostRecentSections = new SparseArray<MostRecentSection>();

        // Intialize adapter
        mAdapter = new MostRecentAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(HISTORY_LOADER_ID, null, mCursorLoaderCallbacks);
    }

    private String getMostRecentSectionTitle(MostRecentSection section) {
        final Resources resources = getActivity().getResources();

        switch (section) {
        case TODAY:
            return resources.getString(R.string.history_today_section);
        case YESTERDAY:
            return resources.getString(R.string.history_yesterday_section);
        case WEEK:
            return resources.getString(R.string.history_week_section);
        case OLDER:
            return resources.getString(R.string.history_older_section);
        }

        throw new IllegalStateException("Unrecognized history section");
    }

    private int getMostRecentSectionsCountBefore(int position) {
        // Account for the number headers before the given position
        int sectionsBefore = 0;

        final int historySectionsCount = mMostRecentSections.size();
        for (int i = 0; i < historySectionsCount; i++) {
            final int sectionPosition = mMostRecentSections.keyAt(i);
            if (sectionPosition > position) {
                break;
            }

            sectionsBefore++;
        }

        return sectionsBefore;
    }

    private MostRecentSection getMostRecentSectionForTime(long from, long time) {
        long delta = from - time;

        if (delta < 0) {
            return MostRecentSection.TODAY;
        }

        if (delta < MS_PER_DAY) {
            return MostRecentSection.YESTERDAY;
        }

        if (delta < MS_PER_WEEK) {
            return MostRecentSection.WEEK;
        }

        return MostRecentSection.OLDER;
    }

    private void loadMostRecentSections(Cursor c) {
        if (c == null || !c.moveToFirst()) {
            return;
        }

        // Clear any history sections that may have been loaded before.
        mMostRecentSections.clear();

        final Date now = new Date();
        now.setHours(0);
        now.setMinutes(0);
        now.setSeconds(0);

        final long today = now.getTime();
        MostRecentSection section = null;

        do {
            final int position = c.getPosition();
            final long time = c.getLong(c.getColumnIndexOrThrow(URLColumns.DATE_LAST_VISITED));
            final MostRecentSection itemSection = getMostRecentSectionForTime(today, time);

            if (section != itemSection) {
                section = itemSection;
                mMostRecentSections.append(position + mMostRecentSections.size(), section);
            }

            // Reached the last section, no need to continue
            if (section == MostRecentSection.OLDER) {
                break;
            }
        } while (c.moveToNext());
    }

    private static class MostRecentCursorLoader extends SimpleCursorLoader {
        // Max number of history results
        private static final int HISTORY_LIMIT = 100;

        public MostRecentCursorLoader(Context context) {
            super(context);
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();
            return BrowserDB.getRecentHistory(cr, HISTORY_LIMIT);
        }
    }

    private class MostRecentAdapter extends SimpleCursorAdapter {
        private static final int ROW_HEADER = 0;
        private static final int ROW_STANDARD = 1;

        private static final int ROW_TYPE_COUNT = 2;

        public MostRecentAdapter(Context context) {
            super(context, -1, null, new String[] {}, new int[] {});
        }

        @Override
        public Object getItem(int position) {
            final int type = getItemViewType(position);

            // Header items are not in the cursor
            if (type == ROW_HEADER) {
                return null;
            }

            return super.getItem(position - getMostRecentSectionsCountBefore(position));
        }

        @Override
        public int getItemViewType(int position) {
            if (mMostRecentSections.get(position) != null) {
                return ROW_HEADER;
            }

            return ROW_STANDARD;
        }

        @Override
        public int getViewTypeCount() {
            // view can be either a standard page row, or a header row
            return ROW_TYPE_COUNT;
        }

        @Override
        public boolean isEnabled(int position) {
            return (getItemViewType(position) == ROW_STANDARD);
        }

        @Override
        public int getCount() {
            // Add the history section headers to the number of reported results.
            return super.getCount() + mMostRecentSections.size();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final int type = getItemViewType(position);

            if (type == ROW_HEADER) {
                final TextView row;
                if (convertView == null) {
                    row = (TextView) mInflater.inflate(R.layout.home_header_row, mList, false);
                } else {
                    row = (TextView) convertView;
                }

                final MostRecentSection section = mMostRecentSections.get(position);
                row.setText(getMostRecentSectionTitle(section));

                return row;
            } else {
                final TwoLinePageRow row;
                if (convertView == null) {
                    row = (TwoLinePageRow) mInflater.inflate(R.layout.home_item_row, mList, false);
                } else {
                    row = (TwoLinePageRow) convertView;
                }

                // Account for the search engines
                position -= getMostRecentSectionsCountBefore(position);

                final Cursor c = getCursor();
                if (!c.moveToPosition(position)) {
                    throw new IllegalStateException("Couldn't move cursor to position " + position);
                }

                row.updateFromCursor(c);

                return row;
            }
        }
    }

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new MostRecentCursorLoader(getActivity());
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            loadMostRecentSections(c);
            mAdapter.swapCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mAdapter.swapCursor(null);
        }
    }
}
