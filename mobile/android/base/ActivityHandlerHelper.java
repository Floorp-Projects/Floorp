/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.ActivityResultHandlerMap;

import android.app.Activity;
import android.content.Intent;

public class ActivityHandlerHelper {
    private static final String LOGTAG = "GeckoActivityHandlerHelper";
    private static final ActivityResultHandlerMap mActivityResultHandlerMap = new ActivityResultHandlerMap();

    private static int makeRequestCode(ActivityResultHandler aHandler) {
        return mActivityResultHandlerMap.put(aHandler);
    }

    public static void startIntent(Intent intent, ActivityResultHandler activityResultHandler) {
        startIntentForActivity(GeckoAppShell.getGeckoInterface().getActivity(), intent, activityResultHandler);
    }

    public static void startIntentForActivity(Activity activity, Intent intent, ActivityResultHandler activityResultHandler) {
        activity.startActivityForResult(intent, mActivityResultHandlerMap.put(activityResultHandler));
    }


    public static boolean handleActivityResult(int requestCode, int resultCode, Intent data) {
        ActivityResultHandler handler = mActivityResultHandlerMap.getAndRemove(requestCode);
        if (handler != null) {
            handler.onActivityResult(resultCode, data);
            return true;
        }
        return false;
    }
}
