/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;

import android.content.Intent;

class AwesomebarResultHandler implements ActivityResultHandler {
    private static final String LOGTAG = "GeckoAwesomebarResultHandler";

    public void onActivityResult(int resultCode, Intent data) {
        if (data != null) {
            String url = data.getStringExtra(AwesomeBar.URL_KEY);
            AwesomeBar.Target target = AwesomeBar.Target.valueOf(data.getStringExtra(AwesomeBar.TARGET_KEY));
            String searchEngine = data.getStringExtra(AwesomeBar.SEARCH_KEY);
            boolean userEntered = data.getBooleanExtra(AwesomeBar.USER_ENTERED_KEY, false);
            if (url != null && url.length() > 0)
                GeckoApp.mAppContext.loadRequest(url, target, searchEngine, userEntered);
        }
    }
}
