/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tab.collections

import android.content.Context
import androidx.lifecycle.LiveData
import androidx.lifecycle.Transformations
import androidx.paging.DataSource
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.ext.writeSnapshotItem
import mozilla.components.feature.tab.collections.adapter.TabAdapter
import mozilla.components.feature.tab.collections.adapter.TabCollectionAdapter
import mozilla.components.feature.tab.collections.db.TabCollectionDatabase
import mozilla.components.feature.tab.collections.db.TabCollectionEntity
import mozilla.components.feature.tab.collections.db.TabEntity
import mozilla.components.support.ktx.java.io.truncateDirectory
import java.io.File
import java.util.UUID

/**
 * A storage implementation that saves snapshots of tabs / sessions in named collections.
 */
class TabCollectionStorage(
    context: Context,
    private val sessionManager: SessionManager,
    private val filesDir: File = context.filesDir
) {
    internal var database: Lazy<TabCollectionDatabase> = lazy { TabCollectionDatabase.get(context) }

    /**
     * Creates a new [TabCollection] and save the state of the given [Session]s in it.
     */
    fun createCollection(title: String, sessions: List<Session> = emptyList()) {
        val entity = TabCollectionEntity(
            title = title,
            updatedAt = System.currentTimeMillis(),
            createdAt = System.currentTimeMillis()
        ).also { entity ->
            entity.id = database.value.tabCollectionDao().insertTabCollection(entity)
        }

        addTabsToCollection(entity, sessions)
    }

    /**
     * Adds the state of the given [Session]s to the [TabCollection].
     */
    fun addTabsToCollection(collection: TabCollection, sessions: List<Session>) {
        val collectionEntity = (collection as TabCollectionAdapter).entity.collection
        addTabsToCollection(collectionEntity, sessions)
    }

    private fun addTabsToCollection(collection: TabCollectionEntity, sessions: List<Session>) {
        sessions.forEach { session ->
            val fileName = UUID.randomUUID().toString()

            val entity = TabEntity(
                title = session.title,
                url = session.url,
                stateFile = fileName,
                tabCollectionId = collection.id!!,
                createdAt = System.currentTimeMillis()
            )

            val snapshot = sessionManager.createSessionSnapshot(session)

            val success = entity.getStateFile(filesDir).writeSnapshotItem(snapshot)
            if (success) {
                database.value.tabDao().insertTab(entity)
            }
        }

        collection.updatedAt = System.currentTimeMillis()
        database.value.tabCollectionDao().updateTabCollection(collection)
    }

    /**
     * Removes the given [Tab] from the [TabCollection].
     */
    fun removeTabFromCollection(collection: TabCollection, tab: Tab) {
        val collectionEntity = (collection as TabCollectionAdapter).entity.collection
        val tabEntity = (tab as TabAdapter).entity

        tabEntity.getStateFile(filesDir)
            .delete()

        database.value.tabDao().deleteTab(tabEntity)

        collectionEntity.updatedAt = System.currentTimeMillis()
        database.value.tabCollectionDao().updateTabCollection(collectionEntity)
    }

    /**
     * Returns all [TabCollection]s as a [DataSource.Factory].
     *
     *  A consuming app can transform the data source into a `LiveData<PagedList>` of when using RxJava2 into a
     * `Flowable<PagedList>` or `Observable<PagedList>`, that can be observed.
     *
     * - https://developer.android.com/topic/libraries/architecture/paging/data
     * - https://developer.android.com/topic/libraries/architecture/paging/ui
     */
    fun getCollectionsPaged(): DataSource.Factory<Int, TabCollection> = database.value
        .tabCollectionDao()
        .getTabCollectionsPaged()
        .map { entity -> TabCollectionAdapter(entity) }

    /**
     * Returns the last [TabCollection] instances (up to [limit]) as a [LiveData] list.
     *
     * @param limit (Optional) Maximum number of [TabCollection] instances that should be returned.
     */
    fun getCollections(limit: Int = 20): LiveData<List<TabCollection>> {
        limit.hashCode()
        return Transformations.map(
            database.value.tabCollectionDao().getTabCollections(limit)
        ) { list ->
            list.map { entity -> TabCollectionAdapter(entity) }
        }
    }

    /**
     * Renames a collection.
     */
    fun renameCollection(collection: TabCollection, title: String) {
        val collectionEntity = (collection as TabCollectionAdapter).entity.collection

        collectionEntity.title = title
        collectionEntity.updatedAt = System.currentTimeMillis()

        database.value.tabCollectionDao().updateTabCollection(collectionEntity)
    }

    /**
     * Removes a collection and all its tabs.
     */
    fun removeCollection(collection: TabCollection) {
        val collectionWithTabs = (collection as TabCollectionAdapter).entity

        database.value
            .tabCollectionDao()
            .deleteTabCollection(collectionWithTabs.collection)

        collectionWithTabs.tabs.forEach { tab ->
            tab.getStateFile(filesDir).delete()
        }
    }

    /**
     * Removes all collections and all tabs.
     */
    fun removeAllCollections() {
        database.value.clearAllTables()

        TabEntity.getStateDirectory(filesDir)
            .truncateDirectory()
    }

    /**
     * Returns the number of tab collections.
     */
    fun getTabCollectionsCount(): Int {
        return database.value.tabCollectionDao().countTabCollections()
    }
}
