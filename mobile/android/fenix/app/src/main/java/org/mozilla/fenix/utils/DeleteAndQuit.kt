/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import org.mozilla.fenix.ext.asActivity
import org.mozilla.fenix.settings.DefaultDeleteBrowsingDataController

/**
 * Deletes selected browsing data and finishes the activity
 */
fun Context.deleteAndQuit(coroutineScope: CoroutineScope) {
    coroutineScope.launch {
        runBlocking {
            val controller =
                DefaultDeleteBrowsingDataController(this@deleteAndQuit, coroutineContext)
            if (Settings.getInstance(this@deleteAndQuit).deleteCacheOnQuit) {
                controller.deleteCachedFiles()
            }
            if (Settings.getInstance(this@deleteAndQuit).deleteTabsOnQuit) {
                controller.deleteTabs()
            }
            if (Settings.getInstance(this@deleteAndQuit).deletePermissionsOnQuit) {
                launch(Dispatchers.IO) {
                    controller.deleteSitePermissions()
                }
            }
            if (Settings.getInstance(this@deleteAndQuit).deleteCookiesOnQuit) {
                controller.deleteCookies()
            }
            if (Settings.getInstance(this@deleteAndQuit).deleteHistoryOnQuit) {
                controller.deleteHistoryAndDOMStorages()
            }
        }
        this@deleteAndQuit.asActivity()?.finish()
    }
}
