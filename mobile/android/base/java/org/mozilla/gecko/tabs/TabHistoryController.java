/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabs;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import android.util.Log;

public class TabHistoryController {
    private static final String LOGTAG = "TabHistoryController";
    private final OnShowTabHistory showTabHistoryListener;

    public static enum HistoryAction {
        ALL,
        BACK,
        FORWARD
    };

    public interface OnShowTabHistory {
        void onShowHistory(List<TabHistoryPage> historyPageList, int toIndex, boolean isPrivate);
    }

    public TabHistoryController(OnShowTabHistory showTabHistoryListener) {
        this.showTabHistoryListener = showTabHistoryListener;
    }

    /**
     * This method will show the history for the current tab.
     */
    public boolean showTabHistory(final Tab tab, final HistoryAction action) {
        final GeckoBundle data = new GeckoBundle(2);
        data.putString("action", action.name());
        data.putInt("tabId", tab.getId());

        final boolean isPrivate = tab.isPrivate();

        EventDispatcher.getInstance().dispatch("Session:GetHistory", data, new EventCallback() {
            @Override
            public void sendSuccess(final Object response) {
                /*
                 * The response from gecko request is of the form
                 * {
                 *   "historyItems" : [
                 *     {
                 *       "title": "google",
                 *       "url": "google.com",
                 *       "selected": false
                 *     }
                 *   ],
                 *   toIndex = 1
                 * }
                 */

                final GeckoBundle data = (GeckoBundle) response;
                final GeckoBundle[] historyItems = data.getBundleArray("historyItems");
                if (historyItems.length == 0) {
                    // Empty history, return without showing the popup.
                    return;
                }

                final List<TabHistoryPage> historyPageList = new ArrayList<>(historyItems.length);
                final int toIndex = data.getInt("toIndex");

                for (GeckoBundle obj : historyItems) {
                    final String title = obj.getString("title");
                    final String url = obj.getString("url");
                    final boolean selected = obj.getBoolean("selected");
                    historyPageList.add(new TabHistoryPage(title, url, selected));
                }

                showTabHistoryListener.onShowHistory(historyPageList, toIndex, isPrivate);
            }

            @Override
            public void sendError(final Object response) {
                Log.e(LOGTAG, "Unexpected error: " + response);
            }
        });
        return true;
    }
}
