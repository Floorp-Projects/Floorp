/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.util.LruCache
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.tabstray.Tab

internal const val INITIAL_NUMBER_OF_TABS = 20
internal const val CACHE_SIZE_MULTIPLIER = 1.5

/**
 * Storage for Browser tabs that need a stable ID for each item in a [RecyclerView.Adapter].
 * This ID is commonly needed by [RecyclerView.Adapter.getItemId] when
 * enabling [RecyclerView.Adapter.setHasStableIds].
 */
internal class TabAdapterIdStorage(initialSize: Int = INITIAL_NUMBER_OF_TABS) {
    private val uniqueTabIds = LruCache<String, Long>(initialSize)
    private var lastUsedSuggestionId = 0L

    /**
     * Returns a unique tab ID for the given [Tab].
     */
    fun getStableId(tab: Tab): Long {
        val key = tab.id
        return uniqueTabIds[key] ?: run {
            lastUsedSuggestionId += 1
            uniqueTabIds.put(key, lastUsedSuggestionId)
            lastUsedSuggestionId
        }
    }

    /**
     * Resizes the internal cache size if the [count] is larger than what is currently available.
     */
    fun resizeCacheIfNeeded(count: Int) {
        val currentMaxSize = uniqueTabIds.maxSize()
        if (count > currentMaxSize) {
            val newMaxSize = (count * CACHE_SIZE_MULTIPLIER).toInt()
            uniqueTabIds.resize(newMaxSize)
        }
    }
}
