/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.dlc;

import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.HardwareUtils;

public class DlcHelper {
    /**
     * Checks whether this functionality is available
     *
     * @param logTag String used when logging to identify the source of a log message
     * @return <code>true</code> if the dlc functionality is available<br>
     *     <code>false</code> otherwise
     */
    static boolean isDlcPossible(String logTag) {
        if (!AppConstants.MOZ_ANDROID_DOWNLOAD_CONTENT_SERVICE) {
            Log.w(logTag, "Download content is not enabled. Stop.");
            return false;
        }

        if (!HardwareUtils.isSupportedSystem()) {
            // This service is running very early before checks in BrowserApp can prevent us from running.
            Log.w(logTag, "System is not supported. Stop.");
            return false;
        }

        return true;
    }
}
