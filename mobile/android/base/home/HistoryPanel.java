/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.EnumSet;
import java.util.List;
import java.util.Locale;

import org.json.JSONException;
import org.json.JSONObject;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoScreenOrientation;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RestrictedProfiles;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract.Combined;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.home.HomeContextMenuInfo.RemoveItemType;
import org.mozilla.gecko.home.HomePager.OnUrlOpenListener;
import org.mozilla.gecko.restrictions.Restriction;
import org.mozilla.gecko.util.ColorUtils;
import org.mozilla.gecko.util.HardwareUtils;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.database.Cursor;
import android.graphics.Typeface;
import android.os.Bundle;
import android.support.v4.content.Loader;
import android.support.v4.widget.CursorAdapter;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.StyleSpan;
import android.text.style.UnderlineSpan;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Fragment that displays recent history in a ListView.
 */
public class HistoryPanel extends HomeFragment {
    // Logging tag name
    private static final String LOGTAG = "GeckoHistoryPanel";

    // For the time sections in history
    private static final long MS_PER_DAY = 86400000;
    private static final long MS_PER_WEEK = MS_PER_DAY * 7;
    private static final List<MostRecentSectionRange> recentSectionTimeOffsetList = new ArrayList<>(MostRecentSection.values().length);

    // Cursor loader ID for history query
    private static final int LOADER_ID_HISTORY = 0;

    // String placeholders to mark formatting.
    private final static String FORMAT_S1 = "%1$s";
    private final static String FORMAT_S2 = "%2$s";

    // Maintain selected range state.
    // Only accessed from the UI thread.
    private static MostRecentSection selected;

    // Adapter for the list of recent history entries.
    private CursorAdapter mAdapter;

    // Adapter for the timeline of history entries.
    private ArrayAdapter<MostRecentSection> mRangeAdapter;

    // The view shown by the fragment.
    private HomeListView mList;
    private HomeListView mRangeList;

    // The button view for clearing browsing history.
    private View mClearHistoryButton;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    // Callbacks used for the search and favicon cursor loaders
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // The time ranges for each section
    public enum MostRecentSection {
        TODAY,
        YESTERDAY,
        WEEK,
        THIS_MONTH,
        MONTH_AGO,
        TWO_MONTHS_AGO,
        THREE_MONTHS_AGO,
        FOUR_MONTHS_AGO,
        FIVE_MONTHS_AGO,
        OLDER_THAN_SIX_MONTHS
    };

    protected interface HistoryUrlProvider {
        public String getURL(int position);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        if (HardwareUtils.isTablet() && GeckoScreenOrientation.getInstance().getAndroidOrientation() == Configuration.ORIENTATION_LANDSCAPE) {
            return inflater.inflate(R.layout.home_history_split_pane_panel, container, false);
        } else {
            return inflater.inflate(R.layout.home_history_panel, container, false);
        }
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mRangeList = (HomeListView) view.findViewById(R.id.range_list);
        mList = (HomeListView) view.findViewById(R.id.list);
        mList.setTag(HomePager.LIST_TAG_HISTORY);

        if (mRangeList != null) {
            mRangeList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
                @Override
                public void onItemClick(AdapterView<?> adapter, View view, int position, long id) {
                    final MostRecentSection rangeItem = (MostRecentSection) adapter.getItemAtPosition(position);
                    if (rangeItem != null) {
                        // Notify data has changed for both range and item adapter.
                        // This will update selected rangeItem item background and the tabs list.
                        // This will also update the selected range along with cursor start and end.
                        selected = rangeItem;
                        mRangeAdapter.notifyDataSetChanged();
                        getLoaderManager().getLoader(LOADER_ID_HISTORY).forceLoad();
                    }
                }
            });
        }

        mList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                final String url = ((HistoryUrlProvider) mAdapter).getURL(position);

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
                info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(Combined.HISTORY_ID));
                info.itemType = RemoveItemType.HISTORY;
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

                        // Send message to Java to clear history.
                        final JSONObject json = new JSONObject();
                        try {
                            json.put("history", true);
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "JSON error", e);
                        }

                        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Sanitize:ClearData", json.toString()));

                        Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.BUTTON, "history");
                    }
                });

                dialogBuilder.show();
            }
        });
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, view, menuInfo);

        if (!RestrictedProfiles.isAllowed(getActivity(), Restriction.DISALLOW_CLEAR_HISTORY)) {
            menu.findItem(R.id.home_remove).setVisible(false);
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        mRangeList = null;
        mList = null;
        mEmptyView = null;
        mClearHistoryButton = null;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        // Reset selection.
        selected = mRangeList == null ? MostRecentSection.THIS_MONTH : MostRecentSection.TODAY;

        // Initialize adapter
        if (mRangeList != null) {
            mAdapter = new HistoryItemAdapter(getActivity(), null, R.layout.home_item_row);
            mRangeAdapter = new HistoryRangeAdapter(getActivity(), R.layout.home_history_range_item);

            mRangeList.setAdapter(mRangeAdapter);
            mList.setAdapter(mAdapter);
        } else {
            mAdapter = new HistoryHeaderListCursorAdapter(getActivity());
            mList.setAdapter(mAdapter);
        }

        // Create callbacks before the initial loader is started
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();

        // Update the section string with current time as reference.
        updateRecentSectionOffset(getActivity());
        loadIfVisible();
    }

    @Override
    protected void loadIfVisible() {
        // Force reload fragment only in tablets.
        if (canLoad() && HardwareUtils.isTablet()) {
            load();
            return;
        }

         super.loadIfVisible();
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_HISTORY, null, mCursorLoaderCallbacks);
    }

    private void updateUiFromCursor(Cursor c) {
        if (c != null && c.getCount() > 0) {
            if (RestrictedProfiles.isAllowed(getActivity(), Restriction.DISALLOW_CLEAR_HISTORY)) {
                mClearHistoryButton.setVisibility(View.VISIBLE);
            }
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

            final TextView emptyHint = (TextView) mEmptyView.findViewById(R.id.home_empty_hint);
            final String hintText = getResources().getString(R.string.home_most_recent_emptyhint);

            final SpannableStringBuilder hintBuilder = formatHintText(hintText);
            if (hintBuilder != null) {
                emptyHint.setText(hintBuilder);
                emptyHint.setMovementMethod(LinkMovementMethod.getInstance());
                emptyHint.setVisibility(View.VISIBLE);
            }

            if (!RestrictedProfiles.isAllowed(getActivity(), Restriction.DISALLOW_PRIVATE_BROWSING)) {
                emptyHint.setVisibility(View.GONE);
            }

            mList.setEmptyView(mEmptyView);
        }
    }

    /**
     * Make Span that is clickable, italicized, and underlined
     * between the string markers <code>FORMAT_S1</code> and
     * <code>FORMAT_S2</code>.
     *
     * @param text String to format
     * @return formatted SpannableStringBuilder, or null if there
     * is not any text to format.
     */
    private SpannableStringBuilder formatHintText(String text) {
        // Set formatting as marked by string placeholders.
        final int underlineStart = text.indexOf(FORMAT_S1);
        final int underlineEnd = text.indexOf(FORMAT_S2);

        // Check that there is text to be formatted.
        if (underlineStart >= underlineEnd) {
            return null;
        }

        final SpannableStringBuilder ssb = new SpannableStringBuilder(text);

        // Set italicization.
        ssb.setSpan(new StyleSpan(Typeface.ITALIC), 0, ssb.length(), 0);

        // Set clickable text.
        final ClickableSpan clickableSpan = new ClickableSpan() {
            @Override
            public void onClick(View widget) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.HOMESCREEN, "hint-private-browsing");
                try {
                    final JSONObject json = new JSONObject();
                    json.put("type", "Menu:Open");
                    EventDispatcher.getInstance().dispatchEvent(json, null);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "Error forming JSON for Private Browsing contextual hint", e);
                }
            }
        };

        ssb.setSpan(clickableSpan, 0, text.length(), 0);

        // Remove underlining set by ClickableSpan.
        final UnderlineSpan noUnderlineSpan = new UnderlineSpan() {
            @Override
            public void updateDrawState(TextPaint textPaint) {
                textPaint.setUnderlineText(false);
            }
        };

        ssb.setSpan(noUnderlineSpan, 0, text.length(), 0);

        // Add underlining for "Private Browsing".
        ssb.setSpan(new UnderlineSpan(), underlineStart, underlineEnd, 0);

        ssb.delete(underlineEnd, underlineEnd + FORMAT_S2.length());
        ssb.delete(underlineStart, underlineStart + FORMAT_S1.length());

        return ssb;
    }

    private static void updateRecentSectionOffset(final Context context) {
        final long now = System.currentTimeMillis();
        final Calendar cal  = Calendar.getInstance();
        cal.set(Calendar.HOUR_OF_DAY, 0);
        cal.set(Calendar.MINUTE, 0);
        cal.set(Calendar.SECOND, 0);
        cal.set(Calendar.MILLISECOND, 1);

        // Calculate the start, end time and display text for the MostRecentSection range.
        recentSectionTimeOffsetList.add(MostRecentSection.TODAY.ordinal(),
                new MostRecentSectionRange(cal.getTimeInMillis(), now, context.getString(R.string.history_today_section)));
        recentSectionTimeOffsetList.add(MostRecentSection.YESTERDAY.ordinal(),
                new MostRecentSectionRange(cal.getTimeInMillis() - MS_PER_DAY, cal.getTimeInMillis(), context.getString(R.string.history_yesterday_section)));
        recentSectionTimeOffsetList.add(MostRecentSection.WEEK.ordinal(),
                new MostRecentSectionRange(cal.getTimeInMillis() - MS_PER_WEEK, now, context.getString(R.string.history_week_section)));

        // Update the calendar to start of next month.
        cal.add(Calendar.MONTH, 1);
        cal.set(Calendar.DAY_OF_MONTH, cal.getMinimum(Calendar.DAY_OF_MONTH));

        // Iterate over the remaining MostRecentSections, to find the start, end and display text.
        for (int i = MostRecentSection.THIS_MONTH.ordinal(); i < MostRecentSection.OLDER_THAN_SIX_MONTHS.ordinal(); i++) {
            final long end = cal.getTimeInMillis();
            cal.add(Calendar.MONTH, -1);
            final long start = cal.getTimeInMillis();
            final String displayName = cal.getDisplayName(Calendar.MONTH, Calendar.LONG, Locale.getDefault());
            recentSectionTimeOffsetList.add(i, new MostRecentSectionRange(start, end, displayName));
        }
        recentSectionTimeOffsetList.add(MostRecentSection.OLDER_THAN_SIX_MONTHS.ordinal(),
                new MostRecentSectionRange(0L, cal.getTimeInMillis(), context.getString(R.string.history_older_section)));
    }

    private static class HistoryCursorLoader extends SimpleCursorLoader {
        // Max number of history results
        private static final int HISTORY_LIMIT = 100;
        private final BrowserDB mDB;

        public HistoryCursorLoader(Context context) {
            super(context);
            mDB = GeckoProfile.get(context).getDB();
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();
            updateRecentSectionOffset(getContext());
            MostRecentSectionRange mostRecentSectionRange = recentSectionTimeOffsetList.get(selected.ordinal());
            return mDB.getRecentHistoryBetweenTime(cr, HISTORY_LIMIT, mostRecentSectionRange.start, mostRecentSectionRange.end);
        }
    }

    protected static String getMostRecentSectionTitle(MostRecentSection section) {
        return recentSectionTimeOffsetList.get(section.ordinal()).displayName;
    }

    protected static MostRecentSection getMostRecentSectionForTime(long time) {
        for (int i = 0; i < MostRecentSection.OLDER_THAN_SIX_MONTHS.ordinal(); i++) {
            if (time > recentSectionTimeOffsetList.get(i).start) {
                return MostRecentSection.values()[i];
            }
        }

        return MostRecentSection.OLDER_THAN_SIX_MONTHS;
    }

    private static class MostRecentSectionRange {
        private final long start;
        private final long end;
        private final String displayName;

        private MostRecentSectionRange(long start, long end, String displayName) {
            this.start = start;
            this.end = end;
            this.displayName = displayName;
        }
    }

    private static class HistoryRangeAdapter extends ArrayAdapter<MostRecentSection> {
        private final Context context;
        private final int resource;

        public HistoryRangeAdapter(Context context, int resource) {
            super(context, resource, MostRecentSection.values());
            this.context = context;
            this.resource = resource;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            final View view;
            if (convertView != null) {
                view = convertView;
            } else {
                final LayoutInflater inflater = LayoutInflater.from(context);
                view = inflater.inflate(resource, parent, false);
                view.setTag(view.findViewById(R.id.range_title));
            }
            final MostRecentSection current = getItem(position);
            final TextView textView = (TextView) view.getTag();
            textView.setText(getMostRecentSectionTitle(current));
            textView.setTextColor(ColorUtils.getColor(context, current == selected ? R.color.text_and_tabs_tray_grey : R.color.disabled_grey));
            textView.setCompoundDrawablesWithIntrinsicBounds(0, 0, current == selected ? R.drawable.home_group_collapsed : 0, 0);
            return view;
        }
    }

    private class CursorLoaderCallbacks extends TransitionAwareCursorLoaderCallbacks {
        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            return new HistoryCursorLoader(getActivity());
        }

        @Override
        public void onLoadFinishedAfterTransitions(Loader<Cursor> loader, Cursor c) {
            mAdapter.swapCursor(c);
            updateUiFromCursor(c);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            super.onLoaderReset(loader);
            mAdapter.swapCursor(null);
        }
    }
}
