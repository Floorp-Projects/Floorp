/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.Date;
import java.util.EnumSet;

import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.BrowserDB.URLColumns;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager.LoaderCallbacks;
import android.support.v4.content.Loader;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Fragment that displays recent history in a ListView.
 */
public class HistoryPanel extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoHistoryPanel";

    // Cursor loader ID for history query
    private static final int LOADER_ID_HISTORY = 0;

    // Adapter for the list of recent history entries.
    private HistoryAdapter mAdapter;

    // The view shown by the fragment.
    private HomeListView mList;

    // The button view for clearing browsing history.
    private View mClearHistoryButton;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // On URL open listener
    private OnUrlOpenListener mUrlOpenListener;

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
        return inflater.inflate(R.layout.home_history_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_HISTORY);

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                position -= mAdapter.getMostRecentSectionsCountBefore(position);
                final Cursor c = mAdapter.getCursor(position);
                final String url = c.getString(c.getColumnIndexOrThrow(URLColumns.URL));

                Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.LIST_ITEM);

                // This item is a TwoLinePageRow, so we allow switch-to-tab.
                mUrlOpenListener.onUrlOpen(url, EnumSet.of(OnUrlOpenListener.Flags.ALLOW_SWITCH_TO_TAB));
            }
        });

        mList.setContextMenuInfoFactory(new HomeContextMenuInfo.Factory() {
            @Override
            public HomeContextMenuInfo makeInfoForCursor(View view, int position, long id, Cursor cursor) {
                final HomeContextMenuInfo info = new HomeContextMenuInfo(view, position, id);
                info.url = cursor.getString(cursor.getColumnIndexOrThrow(Combined.URL));
                info.title = cursor.getString(cursor.getColumnIndexOrThrow(Combined.TITLE));
                info.display = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.DISPLAY));
                info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.HISTORY_ID));
                final int bookmarkIdCol = cursor.getColumnIndexOrThrow(Combined.BOOKMARK_ID);
                if (cursor.isNull(bookmarkIdCol)) {
                    // If this is a combined cursor, we may get a history item without a
                    // bookmark, in which case the bookmarks ID column value will be null.
                    info.bookmarkId =  -1;
                } else {
                    info.bookmarkId = cursor.getInt(bookmarkIdCol);
                }
                return info;
            }
        });
        registerForContextMenu(mList);

        mClearHistoryButton = view.findViewById(R.id.clear_history_button);
        mClearHistoryButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final Context context = getActivity();

                final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(context);
                dialogBuilder.setMessage(R.string.home_clear_history_confirm);

                dialogBuilder.setNegativeButton(R.string.button_cancel, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        dialog.dismiss();
                    }
                });

                dialogBuilder.setPositiveButton(R.string.button_ok, new AlertDialog.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        dialog.dismiss();
                        ThreadUtils.postToBackgroundThread(new Runnable() {
                            @Override
                            public void run() {
                                final ContentResolver cr = context.getContentResolver();
                                BrowserDB.clearHistory(cr);
                            }
                        });

                        Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.BUTTON, "history");
                    }
                });

                dialogBuilder.show();
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mList = null;
        mEmptyView = null;
        mClearHistoryButton = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Intialize adapter
        mAdapter = new HistoryAdapter(getActivity());
        mList.setAdapter(mAdapter);

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
        loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_HISTORY, null, mCursorLoaderCallbacks);
    }

    private static class HistoryCursorLoader extends SimpleCursorLoader {
        // Max number of history results
        private static final int HISTORY_LIMIT = 100;

        public HistoryCursorLoader(Context context) {
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
            mClearHistoryButton.setVisibility(View.VISIBLE);
            return;
        }

        // Cursor is empty, so hide the "Clear browsing history" button,
        // and set the empty view if it hasn't been set already.
        mClearHistoryButton.setVisibility(View.GONE);

        if (mEmptyView == null) {
            // Set empty panel view. We delay this so that the empty view won't flash.
            final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
            mEmptyView = emptyViewStub.inflate();

            final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
            emptyIcon.setImageResource(R.drawable.icon_most_recent_empty);

            final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
            emptyText.setText(R.string.home_most_recent_empty);

            mList.setEmptyView(mEmptyView);
        }
    }

    private static class HistoryAdapter extends MultiTypeCursorAdapter {
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

        public HistoryAdapter(Context context) {
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
            loadMostRecentSections(cursor);
            Cursor oldCursor = super.swapCursor(cursor);
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
            // Clear any history sections that may have been loaded before.
            mMostRecentSections.clear();

            if (c == null || !c.moveToFirst()) {
                return;
            }

            final Date now = new Date();
            now.setHours(0);
            now.setMinutes(0);
            now.setSeconds(0);

            final long today = now.getTime();
            MostRecentSection section = null;

            do {
                final int position = c.getPosition();
                final long time = c.getLong(c.getColumnIndexOrThrow(URLColumns.DATE_LAST_VISITED));
                final MostRecentSection itemSection = HistoryAdapter.getMostRecentSectionForTime(today, time);

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

    private class CursorLoaderCallbacks implements LoaderCallbacks<Cursor> {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new HistoryCursorLoader(getActivity());
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
