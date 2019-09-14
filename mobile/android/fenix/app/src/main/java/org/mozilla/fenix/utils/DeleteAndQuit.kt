/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.app.Activity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.mozilla.fenix.settings.DefaultDeleteBrowsingDataController

/**
 * Deletes selected browsing data and finishes the activity.
 */
fun deleteAndQuit(activity: Activity, coroutineScope: CoroutineScope) {
    coroutineScope.launch {
        val controller = DefaultDeleteBrowsingDataController(activity, coroutineContext)

        if (Settings.getInstance(activity).deleteCacheOnQuit) {
            controller.deleteCachedFiles()
        }
        if (Settings.getInstance(activity).deleteTabsOnQuit) {
            controller.deleteTabs()
        }
        if (Settings.getInstance(activity).deletePermissionsOnQuit) {
            launch(Dispatchers.IO) {
                controller.deleteSitePermissions()
            }
        }
        if (Settings.getInstance(activity).deleteCookiesOnQuit) {
            controller.deleteCookies()
        }
        if (Settings.getInstance(activity).deleteHistoryOnQuit) {
            controller.deleteHistoryAndDOMStorages()
        }

        activity.finish()
    }
}
