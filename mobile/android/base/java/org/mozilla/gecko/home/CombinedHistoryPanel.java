/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.accounts.Account;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.UiThread;
import android.support.v4.app.LoaderManager;
import android.support.v4.content.Loader;
import android.support.v4.widget.SwipeRefreshLayout;
import android.support.v7.widget.DefaultItemAnimator;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
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
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoApp;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.fxa.FxAccountConstants;
import org.mozilla.gecko.fxa.SyncStatusListener;
import org.mozilla.gecko.home.CombinedHistoryPanel.OnPanelLevelChangeListener.PanelLevel;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.widget.HistoryDividerItemDecoration;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class CombinedHistoryPanel extends HomeFragment implements RemoteClientsDialogFragment.RemoteClientsListener {
    private static final String LOGTAG = "GeckoCombinedHistoryPnl";

    private static final String[] STAGES_TO_SYNC_ON_REFRESH = new String[] { "clients", "tabs" };
    private final int LOADER_ID_HISTORY = 0;
    private final int LOADER_ID_REMOTE = 1;

    // String placeholders to mark formatting.
    private final static String FORMAT_S1 = "%1$s";
    private final static String FORMAT_S2 = "%2$s";

    private CombinedHistoryRecyclerView mRecyclerView;
    private CombinedHistoryAdapter mHistoryAdapter;
    private ClientsAdapter mClientsAdapter;
    private RecentTabsAdapter mRecentTabsAdapter;
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    private Bundle mSavedRestoreBundle;

    private PanelLevel mPanelLevel;
    private Button mPanelFooterButton;

    private PanelStateUpdateHandler mPanelStateUpdateHandler;

    // Child refresh layout view.
    protected SwipeRefreshLayout mRefreshLayout;

    // Sync listener that stops refreshing when a sync is completed.
    protected RemoteTabsSyncListener mSyncStatusListener;

    // Reference to the View to display when there are no results.
    private View mHistoryEmptyView;
    private View mClientsEmptyView;
    private View mRecentTabsEmptyView;

    public interface OnPanelLevelChangeListener {
        enum PanelLevel {
        PARENT, CHILD_SYNC, CHILD_RECENT_TABS
    }

        /**
         * Propagates level changes.
         * @param level
         * @return true if level changed, false otherwise.
         */
        boolean changeLevel(PanelLevel level);
    }

    @Override
    public void onCreate(Bundle savedInstance) {
        super.onCreate(savedInstance);

        int cachedRecentTabsCount = 0;
        if (mPanelStateChangeListener != null ) {
            cachedRecentTabsCount = mPanelStateChangeListener.getCachedRecentTabsCount();
        }
        mHistoryAdapter = new CombinedHistoryAdapter(getResources(), cachedRecentTabsCount);
        if (mPanelStateChangeListener != null) {
            mHistoryAdapter.setPanelStateChangeListener(mPanelStateChangeListener);
        }

        mClientsAdapter = new ClientsAdapter(getContext());
        // The RecentTabsAdapter doesn't use a cursor and therefore can't use the CursorLoader's
        // onLoadFinished() callback for updating the panel state when the closed tab count changes.
        // Instead, we provide it with independent callbacks as necessary.
        mRecentTabsAdapter = new RecentTabsAdapter(getContext(),
                mHistoryAdapter.getRecentTabsUpdateHandler(), getPanelStateUpdateHandler());

        mSyncStatusListener = new RemoteTabsSyncListener();
        FirefoxAccounts.addSyncStatusListener(mSyncStatusListener);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_combined_history_panel, container, false);
    }

    @UiThread
    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mRecyclerView = (CombinedHistoryRecyclerView) view.findViewById(R.id.combined_recycler_view);
        setUpRecyclerView();

        mRefreshLayout = (SwipeRefreshLayout) view.findViewById(R.id.refresh_layout);
        setUpRefreshLayout();

        mClientsEmptyView = view.findViewById(R.id.home_clients_empty_view);
        mHistoryEmptyView = view.findViewById(R.id.home_history_empty_view);
        mRecentTabsEmptyView = view.findViewById(R.id.home_recent_tabs_empty_view);
        setUpEmptyViews();

        mPanelFooterButton = (Button) view.findViewById(R.id.history_panel_footer_button);
        mPanelFooterButton.setText(R.string.home_clear_history_button);
        mPanelFooterButton.setOnClickListener(new OnFooterButtonClickListener());

        mRecentTabsAdapter.startListeningForClosedTabs();
        mRecentTabsAdapter.startListeningForHistorySanitize();

        if (mSavedRestoreBundle != null) {
            setPanelStateFromBundle(mSavedRestoreBundle);
            mSavedRestoreBundle = null;
        }
    }

    @UiThread
    private void setUpRecyclerView() {
        if (mPanelLevel == null) {
            mPanelLevel = PanelLevel.PARENT;
        }

        mRecyclerView.setAdapter(mPanelLevel == PanelLevel.PARENT ? mHistoryAdapter :
                mPanelLevel == PanelLevel.CHILD_SYNC ? mClientsAdapter : mRecentTabsAdapter);

        final RecyclerView.ItemAnimator animator = new DefaultItemAnimator();
        animator.setAddDuration(100);
        animator.setChangeDuration(100);
        animator.setMoveDuration(100);
        animator.setRemoveDuration(100);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        mHistoryAdapter.setLinearLayoutManager((LinearLayoutManager) mRecyclerView.getLayoutManager());
        mRecyclerView.setItemAnimator(animator);
        mRecyclerView.addItemDecoration(new HistoryDividerItemDecoration(getContext()));
        mRecyclerView.setOnHistoryClickedListener(mUrlOpenListener);
        mRecyclerView.setOnPanelLevelChangeListener(new OnLevelChangeListener());
        mRecyclerView.setHiddenClientsDialogBuilder(new HiddenClientsHelper());
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                super.onScrolled(recyclerView, dx, dy);
                final LinearLayoutManager llm = (LinearLayoutManager) recyclerView.getLayoutManager();
                if ((mPanelLevel == PanelLevel.PARENT) && (llm.findLastCompletelyVisibleItemPosition() == HistoryCursorLoader.HISTORY_LIMIT)) {
                    Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.LIST, "history_scroll_max");
                }

            }
        });
        registerForContextMenu(mRecyclerView);
    }

    private void setUpRefreshLayout() {
        mRefreshLayout.setColorSchemeResources(R.color.fennec_ui_orange, R.color.action_orange);
        mRefreshLayout.setOnRefreshListener(new RemoteTabsRefreshListener());
        mRefreshLayout.setEnabled(false);
    }

    private void setUpEmptyViews() {
        // Set up history empty view.
        final ImageView historyIcon = (ImageView) mHistoryEmptyView.findViewById(R.id.home_empty_image);
        historyIcon.setVisibility(View.GONE);

        final TextView historyText = (TextView) mHistoryEmptyView.findViewById(R.id.home_empty_text);
        historyText.setText(R.string.home_most_recent_empty);

        final TextView historyHint = (TextView) mHistoryEmptyView.findViewById(R.id.home_empty_hint);

        if (!Restrictions.isAllowed(getActivity(), Restrictable.PRIVATE_BROWSING)) {
            historyHint.setVisibility(View.GONE);
        } else {
            final String hintText = getResources().getString(R.string.home_most_recent_emptyhint);
            final SpannableStringBuilder hintBuilder = formatHintText(hintText);
            if (hintBuilder != null) {
                historyHint.setText(hintBuilder);
                historyHint.setMovementMethod(LinkMovementMethod.getInstance());
                historyHint.setVisibility(View.VISIBLE);
            }
        }

        // Set up Clients empty view.
        final Button syncSetupButton = (Button) mClientsEmptyView.findViewById(R.id.sync_setup_button);
        syncSetupButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "history_syncsetup");
                // This Activity will redirect to the correct Activity as needed.
                final Intent intent = new Intent(FxAccountConstants.ACTION_FXA_GET_STARTED);
                startActivity(intent);
            }
        });

        // Set up Recent Tabs empty view.
        final ImageView recentTabsIcon = (ImageView) mRecentTabsEmptyView.findViewById(R.id.home_empty_image);
        recentTabsIcon.setImageResource(R.drawable.icon_remote_tabs_empty);

        final TextView recentTabsText = (TextView) mRecentTabsEmptyView.findViewById(R.id.home_empty_text);
        recentTabsText.setText(R.string.home_last_tabs_empty);
    }

    @Override
    public void setPanelStateChangeListener(
            PanelStateChangeListener panelStateChangeListener) {
        super.setPanelStateChangeListener(panelStateChangeListener);
        if (mHistoryAdapter != null) {
            mHistoryAdapter.setPanelStateChangeListener(panelStateChangeListener);
        }
    }

    @Override
    public void restoreData(Bundle data) {
        if (mRecyclerView != null) {
            setPanelStateFromBundle(data);
        } else {
            mSavedRestoreBundle = data;
        }
    }

    private void setPanelStateFromBundle(Bundle data) {
        if (data != null && data.getBoolean("goToRecentTabs", false) && mPanelLevel != PanelLevel.CHILD_RECENT_TABS) {
            mPanelLevel = PanelLevel.CHILD_RECENT_TABS;
            mRecyclerView.swapAdapter(mRecentTabsAdapter, true);
            updateEmptyView(PanelLevel.CHILD_RECENT_TABS);
            updateButtonFromLevel();
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mCursorLoaderCallbacks = new CursorLoaderCallbacks();
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
            return BrowserDB.from(mProfile).getTabsAccessor().getRemoteTabsCursor(getContext());
        }
    }

    private static class HistoryCursorLoader extends SimpleCursorLoader {
        // Max number of history results
        public static final int HISTORY_LIMIT = 100;
        private final BrowserDB mDB;

        public HistoryCursorLoader(Context context) {
            super(context);
            mDB = BrowserDB.from(context);
        }

        @Override
        public Cursor loadCursor() {
            final ContentResolver cr = getContext().getContentResolver();
            return mDB.getRecentHistory(cr, HISTORY_LIMIT);
        }
    }

    private class CursorLoaderCallbacks implements LoaderManager.LoaderCallbacks<Cursor> {
        private BrowserDB mDB;    // Pseudo-final: set in onCreateLoader.

        @Override
        public Loader<Cursor> onCreateLoader(int id, Bundle args) {
            if (mDB == null) {
                mDB = BrowserDB.from(getActivity());
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
                    mHistoryAdapter.setHistory(c);
                    updateEmptyView(PanelLevel.PARENT);
                    break;

                case LOADER_ID_REMOTE:
                    final List<RemoteClient> clients = mDB.getTabsAccessor().getClientsFromCursor(c);
                    mHistoryAdapter.getDeviceUpdateHandler().onDeviceCountUpdated(clients.size());
                    mClientsAdapter.setClients(clients);
                    updateEmptyView(PanelLevel.CHILD_SYNC);
                    break;
            }

            updateButtonFromLevel();
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mClientsAdapter.setClients(Collections.<RemoteClient>emptyList());
            mHistoryAdapter.setHistory(null);
        }
    }

    public interface PanelStateUpdateHandler {
        void onPanelStateUpdated(PanelLevel level);
    }

    public PanelStateUpdateHandler getPanelStateUpdateHandler() {
        if (mPanelStateUpdateHandler == null) {
            mPanelStateUpdateHandler = new PanelStateUpdateHandler() {
                @Override
                public void onPanelStateUpdated(PanelLevel level) {
                    updateEmptyView(level);
                    updateButtonFromLevel();
                }
            };
        }
        return mPanelStateUpdateHandler;
    }

    protected class OnLevelChangeListener implements OnPanelLevelChangeListener {
        @Override
        public boolean changeLevel(PanelLevel level) {
            if (level == mPanelLevel) {
                return false;
            }

            mPanelLevel = level;
            switch (level) {
                case PARENT:
                    mRecyclerView.swapAdapter(mHistoryAdapter, true);
                    mRefreshLayout.setEnabled(false);
                    break;
                case CHILD_SYNC:
                    mRecyclerView.swapAdapter(mClientsAdapter, true);
                    mRefreshLayout.setEnabled(mClientsAdapter.getClientsCount() > 0);
                    break;
                case CHILD_RECENT_TABS:
                    mRecyclerView.swapAdapter(mRecentTabsAdapter, true);
                    break;
            }

            updateEmptyView(level);
            updateButtonFromLevel();
            return true;
        }
    }

    private void updateButtonFromLevel() {
        switch (mPanelLevel) {
            case PARENT:
                final boolean historyRestricted = !Restrictions.isAllowed(getActivity(), Restrictable.CLEAR_HISTORY);
                if (historyRestricted || mHistoryAdapter.getItemCount() == mHistoryAdapter.getNumVisibleSmartFolders()) {
                    mPanelFooterButton.setVisibility(View.GONE);
                } else {
                    mPanelFooterButton.setText(R.string.home_clear_history_button);
                    mPanelFooterButton.setVisibility(View.VISIBLE);
                }
                break;
            case CHILD_RECENT_TABS:
                if (mRecentTabsAdapter.getClosedTabsCount() > 1) {
                    mPanelFooterButton.setText(R.string.home_restore_all);
                    mPanelFooterButton.setVisibility(View.VISIBLE);
                } else {
                    mPanelFooterButton.setVisibility(View.GONE);
                }
                break;
            case CHILD_SYNC:
                mPanelFooterButton.setVisibility(View.GONE);
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
                case CHILD_RECENT_TABS:
                    final String telemetryExtra = mRecentTabsAdapter.restoreAllTabs();
                    if (telemetryExtra != null) {
                        Telemetry.sendUIEvent(TelemetryContract.Event.LOAD_URL, TelemetryContract.Method.BUTTON, telemetryExtra);
                    }
                    break;
            }
        }
    }

    private void updateEmptyView(PanelLevel level) {
        boolean showEmptyHistoryView = false;
        boolean showEmptyClientsView = false;
        boolean showEmptyRecentTabsView = false;
        if(mPanelLevel == level){
            switch (mPanelLevel) {
                case PARENT:
                    showEmptyHistoryView = mHistoryAdapter.getItemCount() == mHistoryAdapter.getNumVisibleSmartFolders();
                    mHistoryEmptyView.setVisibility(showEmptyHistoryView ? View.VISIBLE : View.GONE);
                    break;

                case CHILD_SYNC:
                    showEmptyClientsView = mClientsAdapter.getItemCount() == 1;
                    mClientsEmptyView.setVisibility(showEmptyClientsView ? View.VISIBLE : View.GONE);
                    break;

                case CHILD_RECENT_TABS:
                    showEmptyRecentTabsView = mRecentTabsAdapter.getClosedTabsCount() == 0;
                    mRecentTabsEmptyView.setVisibility(showEmptyRecentTabsView ? View.VISIBLE : View.GONE);
                    break;
            }
        }

        final boolean showEmptyView = showEmptyClientsView || showEmptyHistoryView || showEmptyRecentTabsView;
        mRecyclerView.setOverScrollMode(showEmptyView ? View.OVER_SCROLL_NEVER : View.OVER_SCROLL_IF_CONTENT_SCROLLS);
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
                Telemetry.sendUIEvent(TelemetryContract.Event.ACTION, TelemetryContract.Method.BUTTON, "hint_private_browsing");
                try {
                    final JSONObject json = new JSONObject();
                    json.put("type", "Menu:Open");
                    GeckoApp.getEventDispatcher().dispatchEvent(json, null);
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
            mClientsAdapter.removeItem(info.position);
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
        mClientsAdapter.unhideClients(clients);
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

    protected class RemoteTabsRefreshListener implements SwipeRefreshLayout.OnRefreshListener {
        @Override
        public void onRefresh() {
            if (FirefoxAccounts.firefoxAccountsExist(getActivity())) {
                final Account account = FirefoxAccounts.getFirefoxAccount(getActivity());
                FirefoxAccounts.requestImmediateSync(account, STAGES_TO_SYNC_ON_REFRESH, null);
            } else {
                Log.wtf(LOGTAG, "No Firefox Account found; this should never happen. Ignoring.");
                mRefreshLayout.setRefreshing(false);
            }
        }
    }

    protected class RemoteTabsSyncListener implements SyncStatusListener {
        @Override
        public Context getContext() {
            return getActivity();
        }

        @Override
        public Account getAccount() {
            return FirefoxAccounts.getFirefoxAccount(getContext());
        }

        @Override
        public void onSyncStarted() {
        }

        @Override
        public void onSyncFinished() {
            mRefreshLayout.setRefreshing(false);
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        mRecentTabsAdapter.stopListeningForClosedTabs();
        mRecentTabsAdapter.stopListeningForHistorySanitize();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mSyncStatusListener != null) {
            FirefoxAccounts.removeSyncStatusListener(mSyncStatusListener);
            mSyncStatusListener = null;
        }
    }
}
