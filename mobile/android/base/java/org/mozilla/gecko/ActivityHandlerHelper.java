/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.ActivityResultHandlerMap;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class ActivityHandlerHelper {
    private static final String LOGTAG = "GeckoActivityHandlerHelper";
    private static final ActivityResultHandlerMap mActivityResultHandlerMap = new ActivityResultHandlerMap();

    private static int makeRequestCode(ActivityResultHandler aHandler) {
        return mActivityResultHandlerMap.put(aHandler);
    }

    /**
     * Starts the Activity, catching & logging if the Activity fails to start.
     *
     * We catch to prevent callers from passing in invalid Intents and crashing the browser.
     *
     * @return true if the Activity is successfully started, false otherwise.
     */
    public static boolean startIntentAndCatch(final String logtag, final Context context, final Intent intent) {
        try {
            context.startActivity(intent);
            return true;
        } catch (final ActivityNotFoundException e) {
            Log.w(logtag, "Activity not found.", e);
            return false;
        } catch (final SecurityException e) {
            Log.w(logtag, "Forbidden to launch activity.", e);
            return false;
        }
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
