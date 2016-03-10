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
import android.support.v7.widget.RecyclerView;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Telemetry;
import org.mozilla.gecko.TelemetryContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;
import org.mozilla.gecko.widget.DividerItemDecoration;

import java.util.Collections;
import java.util.List;

public class CombinedHistoryPanel extends HomeFragment {
    private static final String LOGTAG = "GeckoCombinedHistoryPnl";
    private final int LOADER_ID_HISTORY = 0;
    private final int LOADER_ID_REMOTE = 1;

    private CombinedHistoryRecyclerView mRecyclerView;
    private CombinedHistoryAdapter mAdapter;
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    private OnPanelLevelChangeListener.PanelLevel mPanelLevel = OnPanelLevelChangeListener.PanelLevel.PARENT;
    private Button mPanelFooterButton;

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
        mAdapter = new CombinedHistoryAdapter(getContext());
        mRecyclerView.setAdapter(mAdapter);
        mRecyclerView.setItemAnimator(new DefaultItemAnimator());
        mRecyclerView.addItemDecoration(new DividerItemDecoration(getContext()));
        mRecyclerView.setOnHistoryClickedListener(mUrlOpenListener);
        mRecyclerView.setOnPanelLevelChangeListener(new OnLevelChangeListener());
        mPanelFooterButton = (Button) view.findViewById(R.id.clear_history_button);
        mPanelFooterButton.setOnClickListener(new OnFooterButtonClickListener());
        mPanelFooterButton.setVisibility(View.VISIBLE);

        // TODO: Check if empty state
        // TODO: Handle date headers.
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
            // TODO: Handle time bracketing by fetching date ranges from cursor
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

        public void onLoadFinished(Loader<Cursor> loader, Cursor c) {
            final int loaderId = loader.getId();
            switch (loaderId) {
                case LOADER_ID_HISTORY:
                    mAdapter.setHistory(c);
                    break;

                case LOADER_ID_REMOTE:
                    final List<RemoteClient> clients = mDB.getTabsAccessor().getClientsFromCursor(c);
                    // TODO: Handle hidden clients
                    mAdapter.setClients(clients);

                    break;
            }

        }

        public void onLoaderReset(Loader<Cursor> c) {
            mAdapter.setClients(Collections.<RemoteClient>emptyList());
            mAdapter.setHistory(null);
        }
    }

    protected class OnLevelChangeListener implements OnPanelLevelChangeListener {
        @Override
        public void onPanelLevelChange(PanelLevel level) {
            mPanelLevel = level;
            switch (mPanelLevel) {
                case PARENT:
                    mPanelFooterButton.setText(R.string.home_clear_history_button);
                    break;
                case CHILD:
                    mPanelFooterButton.setText(R.string.home_open_all);
                    break;
            }
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

                            GeckoAppShell.notifyObservers("Sanitize:ClearData", json.toString());;
                            Telemetry.sendUIEvent(TelemetryContract.Event.SANITIZE, TelemetryContract.Method.BUTTON, "history");
                        }
                    });

                    dialogBuilder.show();
                    break;

                case CHILD:
                    final JSONArray tabUrls = ((CombinedHistoryAdapter) mRecyclerView.getAdapter()).getCurrentChildTabs();
                    if (tabUrls != null) {
                        final JSONObject message = new JSONObject();
                        try {
                            message.put("urls", tabUrls);
                            message.put("shouldNotifyTabsOpenedToJava", false);
                            GeckoAppShell.notifyObservers("Tabs:OpenMultiple", message.toString());;
                        } catch (JSONException e) {
                            Log.e(LOGTAG, "Error making JSON message to open tabs");
                        }
                    }
                    break;
            }
        }
    }
}
