/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.app.ActivityManager

/**
 * This class provides information the running app process without exposing the android framework
 * APIs directly, making easier to test the code that depends on it.
 */
interface ProcessInfoProvider {

    /**
     * Returns true if the current app process is in the foreground.
     */
    fun isForegroundImportance(): Boolean
}

/**
 * @see ProcessInfoProvider
 */
class DefaultProcessInfoProvider : ProcessInfoProvider {

    override fun isForegroundImportance(): Boolean {
        val appProcessInfo = ActivityManager.RunningAppProcessInfo()
        ActivityManager.getMyMemoryState(appProcessInfo)

        return appProcessInfo.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
    }
}
