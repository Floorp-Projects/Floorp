/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.deletebrowsingdata

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.Engine
import org.mozilla.fenix.ext.components
import kotlin.coroutines.CoroutineContext

interface DeleteBrowsingDataController {
    suspend fun deleteTabs()
    suspend fun deleteBrowsingData()
    suspend fun deleteHistoryAndDOMStorages()
    suspend fun deleteCookies()
    suspend fun deleteCachedFiles()
    suspend fun deleteSitePermissions()
}

class DefaultDeleteBrowsingDataController(
    val context: Context,
    val coroutineContext: CoroutineContext = Dispatchers.Main
) : DeleteBrowsingDataController {

    override suspend fun deleteTabs() {
        withContext(coroutineContext) {
            context.components.useCases.tabsUseCases.removeAllTabs.invoke()
        }
    }

    override suspend fun deleteBrowsingData() {
        deleteHistoryAndDOMStorages()
    }

    override suspend fun deleteHistoryAndDOMStorages() {
        withContext(coroutineContext) {
            context.components.core.engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.DOM_STORAGES))
        }
        context.components.core.historyStorage.deleteEverything()
    }

    override suspend fun deleteCookies() {
        withContext(coroutineContext) {
            context.components.core.engine.clearData(
                Engine.BrowsingData.select(
                    Engine.BrowsingData.COOKIES,
                    Engine.BrowsingData.AUTH_SESSIONS
                )
            )
        }
    }

    override suspend fun deleteCachedFiles() {
        withContext(coroutineContext) {
            context.components.core.engine.clearData(
                Engine.BrowsingData.select(Engine.BrowsingData.ALL_CACHES)
            )
        }
    }

    override suspend fun deleteSitePermissions() {
        withContext(coroutineContext) {
            context.components.core.engine.clearData(
                Engine.BrowsingData.select(Engine.BrowsingData.ALL_SITE_SETTINGS)
            )
        }
        context.components.core.permissionStorage.deleteAllSitePermissions()
    }
}
