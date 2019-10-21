/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import android.app.Activity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers.IO
import kotlinx.coroutines.joinAll
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.settings

/**
 * Deletes selected browsing data and finishes the activity.
 */
fun deleteAndQuit(activity: Activity, coroutineScope: CoroutineScope, snackbar: FenixSnackbar?) {
    coroutineScope.launch {
        val settings = activity.settings()
        val controller = DefaultDeleteBrowsingDataController(activity, coroutineContext)

        snackbar?.show()

        DeleteBrowsingDataOnQuitType.values().map { type ->
            launch {
                if (settings.getDeleteDataOnQuit(type)) {
                    controller.deleteType(type)
                }
            }
        }.joinAll()

        snackbar?.dismiss()

        activity.finish()
    }
}

private suspend fun DeleteBrowsingDataController.deleteType(type: DeleteBrowsingDataOnQuitType) {
    when (type) {
        DeleteBrowsingDataOnQuitType.TABS -> deleteTabs()
        DeleteBrowsingDataOnQuitType.HISTORY -> deleteHistoryAndDOMStorages()
        DeleteBrowsingDataOnQuitType.COOKIES -> deleteCookies()
        DeleteBrowsingDataOnQuitType.CACHE -> deleteCachedFiles()
        DeleteBrowsingDataOnQuitType.PERMISSIONS -> withContext(IO) {
            deleteSitePermissions()
        }
    }
}
