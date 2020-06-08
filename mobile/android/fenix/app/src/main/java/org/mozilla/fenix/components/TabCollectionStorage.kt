/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.content.Context
import android.os.StrictMode
import androidx.lifecycle.LiveData
import androidx.paging.DataSource
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tab.collections.Tab
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tab.collections.TabCollectionStorage
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.ktx.android.os.resetAfter
import org.mozilla.fenix.ext.components
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

    var cachedTabCollections = listOf<TabCollection>()

    private val collectionStorage by lazy {
        StrictMode.allowThreadDiskReads().resetAfter {
            TabCollectionStorage(context, sessionManager)
        }
    }

    fun createCollection(title: String, sessions: List<Session>) {
        collectionStorage.createCollection(title, sessions)
        notifyObservers { onCollectionCreated(title, sessions) }
    }

    fun addTabsToCollection(tabCollection: TabCollection, sessions: List<Session>) {
        collectionStorage.addTabsToCollection(tabCollection, sessions)
        notifyObservers { onTabsAdded(tabCollection, sessions) }
    }

    fun getTabCollectionsCount(): Int {
        return collectionStorage.getTabCollectionsCount()
    }

    fun getCollections(limit: Int = 20): LiveData<List<TabCollection>> {
        return collectionStorage.getCollections(limit)
    }

    fun getCollectionsPaged(): DataSource.Factory<Int, TabCollection> {
        return collectionStorage.getCollectionsPaged()
    }

    fun removeCollection(tabCollection: TabCollection) {
        collectionStorage.removeCollection(tabCollection)
    }

    fun removeTabFromCollection(tabCollection: TabCollection, tab: Tab) {
        if (tabCollection.tabs.size == 1) {
            removeCollection(tabCollection)
        } else {
            collectionStorage.removeTabFromCollection(tabCollection, tab)
        }
    }

    fun renameCollection(tabCollection: TabCollection, title: String) {
        collectionStorage.renameCollection(tabCollection, title)
        notifyObservers { onCollectionRenamed(tabCollection, title) }
    }
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
