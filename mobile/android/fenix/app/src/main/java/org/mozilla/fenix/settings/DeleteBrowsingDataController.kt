/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import mozilla.components.concept.engine.Engine
import mozilla.components.feature.tab.collections.TabCollection
import org.mozilla.fenix.ext.components
import kotlin.coroutines.CoroutineContext

interface DeleteBrowsingDataController {
    suspend fun deleteTabs()
    suspend fun deleteBrowsingData()
    suspend fun deleteCollections(collections: List<TabCollection>)
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
        withContext(coroutineContext) {
            context.components.core.engine.clearData(Engine.BrowsingData.all())
        }
        context.components.core.historyStorage.deleteEverything()
    }

    override suspend fun deleteCollections(collections: List<TabCollection>) {
        while (context.components.core.tabCollectionStorage.getTabCollectionsCount() != collections.size) {
            delay(DELAY_IN_MILLIS)
        }

        collections.forEach { context.components.core.tabCollectionStorage.removeCollection(it) }
    }

    override suspend fun deleteCookies() {
        withContext(coroutineContext) {
            context.components.core.engine.clearData(Engine.BrowsingData.select(Engine.BrowsingData.COOKIES))
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

    companion object {
        private const val DELAY_IN_MILLIS = 500L
    }
}
