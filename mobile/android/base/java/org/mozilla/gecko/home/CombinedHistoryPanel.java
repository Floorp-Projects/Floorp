/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.support.v7.widget.DefaultItemAnimator;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.UnderlineSpan;
import android.util.Log;
import android.view.ContextMenu;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.db.RemoteTab;
import org.mozilla.gecko.home.HistorySectionsHelper.SectionDateRange;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.widget.DividerItemDecoration;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class CombinedHistoryPanel extends HomeFragment implements RemoteClientsDialogFragment.RemoteClientsListener {
    private static final String LOGTAG = "GeckoCombinedHistoryPnl";
    private final int LOADER_ID_HISTORY = 0;
    private final int LOADER_ID_REMOTE = 1;

    // Semantic names for the time covered by each section
    public enum SectionHeader {
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
    }

    // Array for the time ranges in milliseconds covered by each section.
    private static final SectionDateRange[] sectionDateRangeArray = new SectionDateRange[SectionHeader.values().length];

    // String placeholders to mark formatting.
    private final static String FORMAT_S1 = "%1$s";
    private final static String FORMAT_S2 = "%2$s";

    private CombinedHistoryRecyclerView mRecyclerView;
    private CombinedHistoryAdapter mAdapter;
    private CursorLoaderCallbacks mCursorLoaderCallbacks;
    private int mSavedParentIndex = -1;

    private OnPanelLevelChangeListener.PanelLevel mPanelLevel = OnPanelLevelChangeListener.PanelLevel.PARENT;
    private Button mPanelFooterButton;
    // Reference to the View to display when there are no results.
    private View mEmptyView;

    public interface OnPanelLevelChangeListener {
        enum PanelLevel {
        PARENT, CHILD
        }

        void onPanelLevelChange(PanelLevel level);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_combined_history_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mRecyclerView = (CombinedHistoryRecyclerView) view.findViewById(R.id.combined_recycler_view);
        mAdapter = new CombinedHistoryAdapter(getContext(), mSavedParentIndex);
        mRecyclerView.setAdapter(mAdapter);
        mRecyclerView.setItemAnimator(new DefaultItemAnimator());
        mRecyclerView.addItemDecoration(new DividerItemDecoration(getContext()));
        mRecyclerView.setOnHistoryClickedListener(mUrlOpenListener);
        mRecyclerView.setOnPanelLevelChangeListener(new OnLevelChangeListener());
        mRecyclerView.setHiddenClientsDialogBuilder(new HiddenClientsHelper());
        registerForContextMenu(mRecyclerView);

        mPanelFooterButton = (Button) view.findViewById(R.id.clear_history_button);
        mPanelFooterButton.setOnClickListener(new OnFooterButtonClickListener());
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (isVisible()) {
            // The parent stack is saved just so that the folder state can be
            // restored on rotation.
            mSavedParentIndex = mAdapter.getParentIndex();
        }
    }

    @Override
    protected void load() {
        getLoaderManager().initLoader(LOADER_ID_HISTORY, null, mCursorLoaderCallbacks);
        getLoaderManager().initLoader(LOADER_ID_REMOTE, null, mCursorLoaderCallbacks);
    }

    private static class RemoteTabsCursorLoader extends SimpleCursorLoader {
        private final GeckoProfile mProfile;

        public RemoteTabsCursorLoader(Context context) {
            super(context);
            mProfile = GeckoProfile.get(context);
        }

        @Override
        public Cursor loadCursor() {
            return mProfile.getDB().getTabsAccessor().getRemoteTabsCursor(getContext());
        }
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
            HistorySectionsHelper.updateRecentSectionOffset(getContext().getResources(), sectionDateRangeArray);
            return mDB.getRecentHistory(cr, HISTORY_LIMIT);
        }
    }

    protected static String getSectionHeaderTitle(SectionHeader section) {
        return sectionDateRangeArray[section.ordinal()].displayName;
    }

    protected static SectionHeader getSectionFromTime(long time) {
        for (int i = 0; i < SectionHeader.OLDER_THAN_SIX_MONTHS.ordinal(); i++) {
            if (time > sectionDateRangeArray[i].start) {
                return SectionHeader.values()[i];
            }
        }

        return SectionHeader.OLDER_THAN_SIX_MONTHS;
    }

    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        private BrowserDB mDB;    // Pseudo-final: set in onCreateLoader.

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (mDB == null) {
                mDB = GeckoProfile.get(getActivity()).getDB();
            }

            switch (id) {
                case LOADER_ID_HISTORY:
                    return new HistoryCursorLoader(getContext());
                case LOADER_ID_REMOTE:
                    return new RemoteTabsCursorLoader(getContext());
                default:
                    Log.e(LOGTAG, "Unknown loader id!");
                    return null;
            }
        }

        @Override
        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch (loaderId) {
                case LOADER_ID_HISTORY:
                    mAdapter.setHistory(c);
                    break;

                case LOADER_ID_REMOTE:
                    final List<RemoteClient> clients = mDB.getTabsAccessor().getClientsFromCursor(c);

                    mAdapter.setClients(clients);
                    break;
            }

            // Check and set empty state.
            updateButtonFromLevel(mPanelLevel);
            updateEmptyView(mAdapter.getItemCount() == 0);
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mAdapter.setClients(Collections.<RemoteClient>emptyList());
            mAdapter.setHistory(null);
        }
    }

    protected class OnLevelChangeListener implements OnPanelLevelChangeListener {
        @Override
        public void onPanelLevelChange(PanelLevel level) {
            updateButtonFromLevel(level);
        }
    }

    private void updateButtonFromLevel(OnPanelLevelChangeListener.PanelLevel level) {
        mPanelLevel = level;
        switch (level) {
            case CHILD:
                mPanelFooterButton.setVisibility(View.VISIBLE);
                mPanelFooterButton.setText(R.string.home_open_all);
                break;
            case PARENT:
                final boolean historyRestricted = !Restrictions.isAllowed(getActivity(), Restrictable.CLEAR_HISTORY);
                if (historyRestricted || !mAdapter.containsHistory()) {
                    mPanelFooterButton.setVisibility(View.GONE);
                } else {
                    mPanelFooterButton.setVisibility(View.VISIBLE);
                    mPanelFooterButton.setText(R.string.home_clear_history_button);
                }
                break;
        }
    }

    private class OnFooterButtonClickListener implements View.OnClickListener {
        @Override
        public void onClick(View view) {
            switch (mPanelLevel) {
                case PARENT:
                    final AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(getActivity());
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

                            GeckoAppShell.notifyObservers("Sanitize:ClearData", json.toString());
                            Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.BUTTON, "history");
                        }
                    });

                    dialogBuilder.show();
                    break;

                case CHILD:
                    final JSONArray tabUrls = mAdapter.getCurrentChildTabs();
                    if (tabUrls != null) {
                        final JSONObject message = new JSONObject();
                        try {
                            message.put("urls", tabUrls);
                            message.put("shouldNotifyTabsOpenedToJava", false);
                            GeckoAppShell.notifyObservers("Tabs:OpenMultiple", message.toString());
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Error making JSON message to open tabs");
                        }
                    }
                    break;
            }
        }
    }

    private void updateEmptyView(boolean isEmpty) {
        if (isEmpty) {
            if (mEmptyView == null) {
                // Set empty panel view if it needs to be shown and hasn't been inflated.
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

                if (!Restrictions.isAllowed(getActivity(), Restrictable.PRIVATE_BROWSING)) {
                    emptyHint.setVisibility(View.GONE);
                }
                mEmptyView.setVisibility(View.VISIBLE);
            } else {
                if (mEmptyView != null) {
                    mEmptyView.setVisibility(View.GONE);
                }
            }
        }
    }
    /**
     * Make Span that is clickable, and underlined
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

        // Set clickable text.
        final ClickableSpan clickableSpan = new ClickableSpan() {
            @Override
            public void onClick(View widget) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.PANEL, "hint-private-browsing");
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

    @Override
    public void onCreateContextMenu(ContextMenu menu, View view, ContextMenu.ContextMenuInfo menuInfo) {
        if (!(menuInfo instanceof RemoteTabsClientContextMenuInfo)) {
            // Long pressed item was not a RemoteTabsGroup item. Superclass
            // can handle this.
            super.onCreateContextMenu(menu, view, menuInfo);
            return;
        }

        // Long pressed item was a remote client; provide the appropriate menu.
        final MenuInflater inflater = new MenuInflater(view.getContext());
        inflater.inflate(R.menu.home_remote_tabs_client_contextmenu, menu);

        final RemoteTabsClientContextMenuInfo info = (RemoteTabsClientContextMenuInfo) menuInfo;
        menu.setHeaderTitle(info.client.name);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        if (super.onContextItemSelected(item)) {
            // HomeFragment was able to handle to selected item.
            return true;
        }

        final ContextMenu.ContextMenuInfo menuInfo = item.getMenuInfo();
        if (!(menuInfo instanceof RemoteTabsClientContextMenuInfo)) {
            return false;
        }

        final RemoteTabsClientContextMenuInfo info = (RemoteTabsClientContextMenuInfo) menuInfo;

        final int itemId = item.getItemId();
        if (itemId == R.id.home_remote_tabs_hide_client) {
            mAdapter.removeItem(info.position);
            return true;
        }

        return false;
    }

    interface DialogBuilder<E> {
        void createAndShowDialog(List<E> items);
    }

    protected class HiddenClientsHelper implements DialogBuilder<RemoteClient> {
        @Override
        public void createAndShowDialog(List<RemoteClient> clientsList) {
                        final RemoteClientsDialogFragment dialog = RemoteClientsDialogFragment.newInstance(
                    getResources().getString(R.string.home_remote_tabs_hidden_devices_title),
                    getResources().getString(R.string.home_remote_tabs_unhide_selected_devices),
                    RemoteClientsDialogFragment.ChoiceMode.MULTIPLE, new ArrayList<>(clientsList));
            dialog.setTargetFragment(CombinedHistoryPanel.this, 0);
            dialog.show(getActivity().getSupportFragmentManager(), "show-clients");
        }


    }

    @Override
    public void onClients(List<RemoteClient> clients) {
        mAdapter.unhideClients(clients);
    }

    /**
     * Stores information regarding the creation of the context menu for a remote client.
     */
    protected static class RemoteTabsClientContextMenuInfo extends HomeContextMenuInfo {
        protected final RemoteClient client;

        public RemoteTabsClientContextMenuInfo(View targetView, int position, long id, RemoteClient client) {
            super(targetView, position, id);
            this.client = client;
        }
    }

    protected static HomeContextMenuInfo populateHistoryInfoFromCursor(HomeContextMenuInfo info, Cursor cursor) {
        info.url = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.URL));
        info.title = cursor.getString(cursor.getColumnIndexOrThrow(BrowserContract.Combined.TITLE));
        info.historyId = cursor.getInt(cursor.getColumnIndexOrThrow(BrowserContract.Combined.HISTORY_ID));
        info.itemType = HomeContextMenuInfo.RemoveItemType.HISTORY;
        final int bookmarkIdCol = cursor.getColumnIndexOrThrow(BrowserContract.Combined.BOOKMARK_ID);
        if (cursor.isNull(bookmarkIdCol)) {
            // If this is a combined cursor, we may get a history item without a
            // bookmark, in which case the bookmarks ID column value will be null.
            info.bookmarkId =  -1;
        } else {
            info.bookmarkId = cursor.getInt(bookmarkIdCol);
        }
        return info;
    }

    protected static HomeContextMenuInfo populateChildInfoFromTab(HomeContextMenuInfo info, RemoteTab tab) {
        info.url = tab.url;
        info.title = tab.title;
        return info;
    }
}
