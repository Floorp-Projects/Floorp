/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.v4.content.Loader;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.R;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.RemoteClient;

import java.util.List;

public class CombinedHistoryPanel extends HomeFragment {
    private static final String LOGTAG = "GeckoCombinedHistoryPnl";
    private final int LOADER_ID_HISTORY = 0;
    private final int LOADER_ID_REMOTE = 1;

    private CombinedHistoryRecyclerView mRecyclerView;
    private CombinedHistoryAdapter mAdapter;
    private CursorLoaderCallbacks mCursorLoaderCallbacks;

    // The button view for clearing browsing history.
    private View mClearHistoryButton;

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
        mRecyclerView.setOnHistoryClickedListener(mUrlOpenListener);
        mClearHistoryButton = view.findViewById(R.id.clear_history_button);
        // TODO: link up click handler for clear history button
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

    private class CursorLoaderCallbacks extends TransitionAwareCursorLoaderCallbacks {
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

        protected void onLoadFinishedAfterTransitions(Loader<Cursor> loader, Cursor c) {
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
    }
}
