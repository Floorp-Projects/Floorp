/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.search.autocomplete;

import android.app.Activity;
import android.database.Cursor;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;

import java.util.ArrayList;

/**
 * A single entry point for querying all agents.
 * <p/>
 * An agent is responsible for querying some underlying data source. It could be a
 * flat file, or a REST endpoint, or a content provider.
 */
class AutoCompleteAgentManager {

    private final Handler mainUiHandler;
    private final Handler localHandler;
    private final AutoCompleteWordListAgent autoCompleteWordListAgent;

    public AutoCompleteAgentManager(Activity activity, Handler mainUiHandler) {
        HandlerThread thread = new HandlerThread("org.mozilla.search.autocomplete.SuggestionAgent");
        // TODO: Where to kill this thread?
        thread.start();
        Log.i("AUTOCOMPLETE", "Starting thread");
        this.mainUiHandler = mainUiHandler;
        localHandler = new SuggestionMessageHandler(thread.getLooper());
        autoCompleteWordListAgent = new AutoCompleteWordListAgent(activity);
    }

    /**
     * Process the next incoming query.
     */
    public void search(String queryString) {
        // TODO check if there's a pending search.. not sure how to handle that.
        localHandler.sendMessage(localHandler.obtainMessage(0, queryString));
    }

    /**
     * This background thread runs the queries; the results get sent back through mainUiHandler
     * <p/>
     * TODO: Refactor this wordlist search and add other search providers (eg: Yahoo)
     */
    private class SuggestionMessageHandler extends Handler {

        private SuggestionMessageHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);
            if (null == msg.obj) {
                return;
            }

            Cursor cursor =
                    autoCompleteWordListAgent.getWordMatches(((String) msg.obj).toLowerCase());
            ArrayList<AutoCompleteModel> res = new ArrayList<AutoCompleteModel>();

            if (null == cursor) {
                return;
            }

            for (cursor.moveToFirst(); !cursor.isAfterLast(); cursor.moveToNext()) {
                res.add(new AutoCompleteModel(cursor.getString(
                        cursor.getColumnIndex(AutoCompleteWordListAgent.COL_WORD))));
            }


            mainUiHandler.sendMessage(Message.obtain(mainUiHandler, 0, res));
        }

    }

}
