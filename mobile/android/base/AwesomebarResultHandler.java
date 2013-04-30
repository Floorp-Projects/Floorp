/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;

import android.content.Intent;
import android.util.Log;

class AwesomebarResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoAwesomebarResultHandler";

    @Override
    public void onActivityResult(int resultCode, Intent data) {
        if (data != null) {
            String tab = data.getStringExtra(AwesomeBar.TAB_KEY);
            if (tab != null) {
                Tabs.getInstance().selectTab(Integer.parseInt(tab));
                return;
            }

            String url = data.getStringExtra(AwesomeBar.URL_KEY);
            AwesomeBar.Target target = AwesomeBar.Target.valueOf(data.getStringExtra(AwesomeBar.TARGET_KEY));
            String searchEngine = data.getStringExtra(AwesomeBar.SEARCH_KEY);
            if (url != null && url.length() > 0) {
                int flags = Tabs.LOADURL_NONE;
                if (target == AwesomeBar.Target.NEW_TAB) {
                    flags |= Tabs.LOADURL_NEW_TAB;
                }
                if (data.getBooleanExtra(AwesomeBar.USER_ENTERED_KEY, false)) {
                    flags |= Tabs.LOADURL_USER_ENTERED;
                }
                Tabs.getInstance().loadUrl(url, searchEngine, -1, flags);
            }
        }
    }
}
