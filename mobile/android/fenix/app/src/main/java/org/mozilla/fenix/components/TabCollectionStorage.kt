/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.StrictMode
import androidx.lifecycle.LiveData
import androidx.lifecycle.asLiveData
import androidx.paging.DataSource
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.TabCollectionStorage
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.resetPoliciesAfter
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.sessioncontrol.viewholders.CollectionViewHolder
import org.mozilla.fenix.utils.Mockable

@Mockable
class TabCollectionStorage(
    private val context: Context,
    private val sessionManager: SessionManager,
    private val delegate: Observable<Observer> = ObserverRegistry()
) : Observable<org.mozilla.fenix.components.TabCollectionStorage.Observer> by delegate {

    /**
     * Interface to be implemented by classes that want to observe the storage
     */
    interface Observer {
        /**
         * A collection has been created
         */
        fun onCollectionCreated(title: String, sessions: List<Session>) = Unit

        /**
         *  Tab(s) have been added to collection
         */
        fun onTabsAdded(tabCollection: TabCollection, sessions: List<Session>) = Unit

        /**
         *  Collection has been renamed
         */
        fun onCollectionRenamed(tabCollection: TabCollection, title: String) = Unit
    }

    private val ioScope = CoroutineScope(Dispatchers.IO)
    var cachedTabCollections = listOf<TabCollection>()

    private val collectionStorage by lazy {
        StrictMode.allowThreadDiskReads().resetPoliciesAfter {
            TabCollectionStorage(context, sessionManager)
        }
    }

    suspend fun createCollection(title: String, sessions: List<Session>) = ioScope.launch {
        collectionStorage.createCollection(title, sessions)
        notifyObservers { onCollectionCreated(title, sessions) }
    }.join()

    suspend fun addTabsToCollection(tabCollection: TabCollection, sessions: List<Session>) = ioScope.launch {
        collectionStorage.addTabsToCollection(tabCollection, sessions)
        notifyObservers { onTabsAdded(tabCollection, sessions) }
    }.join()

    fun getTabCollectionsCount(): Int {
        return collectionStorage.getTabCollectionsCount()
    }

    fun getCollections(): LiveData<List<TabCollection>> {
        return collectionStorage.getCollections().asLiveData()
    }

    fun getCollectionsPaged(): DataSource.Factory<Int, TabCollection> {
        return collectionStorage.getCollectionsPaged()
    }

    suspend fun removeCollection(tabCollection: TabCollection) = ioScope.launch {
        collectionStorage.removeCollection(tabCollection)
    }.join()

    suspend fun removeTabFromCollection(tabCollection: TabCollection, tab: Tab) = ioScope.launch {
        collectionStorage.removeTabFromCollection(tabCollection, tab)
    }.join()

    suspend fun renameCollection(tabCollection: TabCollection, title: String) = ioScope.launch {
        collectionStorage.renameCollection(tabCollection, title)
        notifyObservers { onCollectionRenamed(tabCollection, title) }
    }.join()
}

fun TabCollection.description(context: Context): String {
    return this.tabs
        .map { it.url.toShortUrl(context.components.publicSuffixList) }
        .map {
            if (it.length > CollectionViewHolder.maxTitleLength) {
                it.substring(
                    0,
                    CollectionViewHolder.maxTitleLength
                ) + "…"
            } else {
                it
            }
        }
        .distinct()
        .joinToString(", ")
}
