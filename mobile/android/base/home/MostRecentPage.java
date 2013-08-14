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
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import java.util.Date;
import java.util.EnumSet;

/**
 * Fragment that displays recent history in a ListView.
 */
public class MostRecentPage extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoMostRecentPage";

    // Cursor loader ID for history query
    private static final int LOADER_ID_HISTORY = 0;

    // Adapter for the list of search results
    private MostRecentAdapter mAdapter;

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
    }

    @Override
    public void onDetach() {
        super.onDetach();
        mUrlOpenListener = null;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_most_recent_page, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mTitle = (TextView) view.findViewById(R.id.title);
        if (mTitle != null) {
            mTitle.setText(R.string.home_most_recent_title);
        }

        mList = (ListView) view.findViewById(R.id.list);
        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                position -= mAdapter.getMostRecentSectionsCountBefore(position);
                final Cursor c = mAdapter.getCursor(position);
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
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        final Activity activity = getActivity();

        // Intialize adapter
        mAdapter = new MostRecentAdapter(activity);
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks(activity, getLoaderManager());
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_HISTORY, null, mCursorLoaderCallbacks);
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

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            if (mTitle != null) {
                mTitle.setVisibility(View.VISIBLE);
            }
            return;
        }

        // Cursor is empty, so hide the title and set the empty view if it hasn't been set already.
        if (mTitle != null) {
            mTitle.setVisibility(View.VISIBLE);
        }
        if (mEmptyView == null) {
            // Set empty page view. We delay this so that the empty view won't flash.
            ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_most_recent_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_most_recent_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    private static class MostRecentAdapter extends MultiTypeCursorAdapter {
        private static final int ROW_HEADER = 0;
        private static final int ROW_STANDARD = 1;

        private static final int[] VIEW_TYPES = new int[] { ROW_STANDARD, ROW_HEADER };
        private static final int[] LAYOUT_TYPES = new int[] { R.layout.home_item_row, R.layout.home_header_row };

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

        private final Context mContext;

        // Maps headers in the list with their respective sections
        private final SparseArray<MostRecentSection> mMostRecentSections;

        public MostRecentAdapter(Context context) {
            super(context, null, VIEW_TYPES, LAYOUT_TYPES);

            mContext = context;

            // Initialize map of history sections
            mMostRecentSections = new SparseArray<MostRecentSection>();
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
        public boolean isEnabled(int position) {
            return (getItemViewType(position) == ROW_STANDARD);
        }

        @Override
        public int getCount() {
            // Add the history section headers to the number of reported results.
            return super.getCount() + mMostRecentSections.size();
        }

        @Override
        public Cursor swapCursor(Cursor cursor) {
            Cursor oldCursor = super.swapCursor(cursor);
            loadMostRecentSections(cursor);
            return oldCursor;
        }

        @Override
        public void bindView(View view, Context context, int position) {
            final int type = getItemViewType(position);

            if (type == ROW_HEADER) {
                final MostRecentSection section = mMostRecentSections.get(position);
                final TextView row = (TextView) view;
                row.setText(getMostRecentSectionTitle(section));
            } else {
                // Account for the most recent section headers
                position -= getMostRecentSectionsCountBefore(position);
                final Cursor c = getCursor(position);
                final TwoLinePageRow row = (TwoLinePageRow) view;
                row.updateFromCursor(c);
            }
        }

        private String getMostRecentSectionTitle(MostRecentSection section) {
            switch (section) {
            case TODAY:
                return mContext.getString(R.string.history_today_section);
            case YESTERDAY:
                return mContext.getString(R.string.history_yesterday_section);
            case WEEK:
                return mContext.getString(R.string.history_week_section);
            case OLDER:
                return mContext.getString(R.string.history_older_section);
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

        private static MostRecentSection getMostRecentSectionForTime(long from, long time) {
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
                final MostRecentSection itemSection = MostRecentAdapter.getMostRecentSectionForTime(today, time);

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
    }

    private class CursorLoaderCallbacks extends HomeCursorLoaderCallbacks {
        public CursorLoaderCallbacks(Context context, LoaderManager loaderManager) {
            super(context, loaderManager);
        }

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (id == LOADER_ID_HISTORY) {
                return new MostRecentCursorLoader(getActivity());
            } else {
                return super.onCreateLoader(id, args);
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            if (loader.getId() == LOADER_ID_HISTORY) {
                mAdapter.swapCursor(c);
                updateUiFromCursor(c);
                loadFavicons(c);
            } else {
                super.onLoadFinished(loader, c);
            }
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            if (loader.getId() == LOADER_ID_HISTORY) {
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
