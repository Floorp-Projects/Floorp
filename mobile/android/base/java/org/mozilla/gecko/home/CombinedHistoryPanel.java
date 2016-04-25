/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
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
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.RemoteClientsDialogFragment;
import org.mozilla.gecko.restrictions.Restrictions;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.restrictions.Restrictable;
import org.mozilla.gecko.widget.DividerItemDecoration;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class CombinedHistoryPanel extends HomeFragment implements RemoteClientsDialogFragment.RemoteClientsListener {
    private static final String LOGTAG = "GeckoCombinedHistoryPnl";
    private final int LOADER_ID_HISTORY = 0;
    private final int LOADER_ID_REMOTE = 1;

    // String placeholders to mark formatting.
    private final static String FORMAT_S1 = "%1$s";
    private final static String FORMAT_S2 = "%2$s";

    // Number of smart folders for determining practical empty state.
    public static final int NUM_SMART_FOLDERS = 1;

    private CombinedHistoryRecyclerView mRecyclerView;
    private CombinedHistoryAdapter mHistoryAdapter;
    private ClientsAdapter mClientsAdapter;
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    private OnPanelLevelChangeListener.PanelLevel mPanelLevel;
    private Button mPanelFooterButton;

    // Reference to the View to display when there are no results.
    private View mEmptyView;

    public interface OnPanelLevelChangeListener {
        enum PanelLevel {
        PARENT, CHILD
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

        mHistoryAdapter = new CombinedHistoryAdapter(getResources());
        mClientsAdapter = new ClientsAdapter(getContext());
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.home_combined_history_panel, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mRecyclerView = (CombinedHistoryRecyclerView) view.findViewById(R.id.combined_recycler_view);
        setUpRecyclerView();

        mPanelFooterButton = (Button) view.findViewById(R.id.clear_history_button);
        mPanelFooterButton.setOnClickListener(new OnFooterButtonClickListener());
    }

    private void setUpRecyclerView() {
        if (mPanelLevel == null) {
            mPanelLevel = OnPanelLevelChangeListener.PanelLevel.PARENT;
        }

        mRecyclerView.setAdapter(mPanelLevel == OnPanelLevelChangeListener.PanelLevel.PARENT ? mHistoryAdapter : mClientsAdapter);

        mRecyclerView.setItemAnimator(new DefaultItemAnimator());
        mRecyclerView.addItemDecoration(new DividerItemDecoration(getContext()));
        mRecyclerView.setOnHistoryClickedListener(mUrlOpenListener);
        mRecyclerView.setOnPanelLevelChangeListener(new OnLevelChangeListener());
        mRecyclerView.setHiddenClientsDialogBuilder(new HiddenClientsHelper());
        registerForContextMenu(mRecyclerView);
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
            return mDB.getRecentHistory(cr, HISTORY_LIMIT);
        }
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
            boolean showEmptyView = false;
            switch (loaderId) {
                case LOADER_ID_HISTORY:
                    mHistoryAdapter.setHistory(c);
                    showEmptyView = mHistoryAdapter.getItemCount() == NUM_SMART_FOLDERS;
                    break;

                case LOADER_ID_REMOTE:
                    final List<RemoteClient> clients = mDB.getTabsAccessor().getClientsFromCursor(c);
                    mHistoryAdapter.getDeviceUpdateHandler().onDeviceCountUpdated(clients.size());
                    mClientsAdapter.setClients(clients);
                    showEmptyView = mClientsAdapter.getItemCount() == 1;
                    break;
            }

            updateEmptyView(showEmptyView, mPanelLevel);
            updateButtonFromLevel();
        }

        @Override
        public void onLoaderReset(Loader<Cursor> loader) {
            mClientsAdapter.setClients(Collections.<RemoteClient>emptyList());
            mHistoryAdapter.setHistory(null);
        }
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
                    mRecyclerView.swapAdapter(mHistoryAdapter, false);
                    break;
                case CHILD:
                    mRecyclerView.swapAdapter(mClientsAdapter, false);
                    break;
            }
            updateButtonFromLevel();
            return true;
        }
    }

    private void updateButtonFromLevel() {
        switch (mPanelLevel) {
            case PARENT:
                final boolean historyRestricted = !Restrictions.isAllowed(getActivity(), Restrictable.CLEAR_HISTORY);
                if (historyRestricted || mHistoryAdapter.getItemCount() <= NUM_SMART_FOLDERS) {
                    mPanelFooterButton.setVisibility(View.GONE);
                } else {
                    mPanelFooterButton.setVisibility(View.VISIBLE);
                }
                break;
            case CHILD:
                mPanelFooterButton.setVisibility(View.GONE);
                break;
        }
    }

    private class OnFooterButtonClickListener implements View.OnClickListener {
        @Override
        public void onClick(View view) {

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
        }
    }

    private void updateEmptyView(boolean showEmptyState, OnPanelLevelChangeListener.PanelLevel level) {
        // TODO: Add Sync empty state.
        if (showEmptyState) {
            if (mEmptyView == null) {
                // Set empty panel view if it needs to be shown and hasn't been inflated.
                final ViewStub emptyViewStub = (ViewStub) getView().findViewById(R.id.home_empty_view_stub);
                mEmptyView = emptyViewStub.inflate();

                final boolean isParentLevel = level == OnPanelLevelChangeListener.PanelLevel.PARENT;
                final ImageView emptyIcon = (ImageView) mEmptyView.findViewById(R.id.home_empty_image);
                emptyIcon.setImageResource(isParentLevel ? R.drawable.icon_most_recent_empty : R.drawable.icon_remote_tabs_empty);

                final TextView emptyText = (TextView) mEmptyView.findViewById(R.id.home_empty_text);
                emptyText.setText(R.string.home_most_recent_empty);

                final TextView emptyHint = (TextView) mEmptyView.findViewById(R.id.home_empty_hint);

                if (!Restrictions.isAllowed(getActivity(), Restrictable.PRIVATE_BROWSING) || !isParentLevel) {
                    emptyHint.setVisibility(View.GONE);
                } else {
                    final String hintText = getResources().getString(R.string.home_most_recent_emptyhint);
                    final SpannableStringBuilder hintBuilder = formatHintText(hintText);
                    if (hintBuilder != null) {
                        emptyHint.setText(hintBuilder);
                        emptyHint.setMovementMethod(LinkMovementMethod.getInstance());
                        emptyHint.setVisibility(View.VISIBLE);
                    }
                }
            }
            mEmptyView.setVisibility(View.VISIBLE);
        } else {
            if (mEmptyView != null) {
                mEmptyView.setVisibility(View.GONE);
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
}
