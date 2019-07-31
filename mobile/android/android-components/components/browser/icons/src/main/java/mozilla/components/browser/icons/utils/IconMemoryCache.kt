/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.utils

import android.graphics.Bitmap
import android.util.LruCache
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest
import mozilla.components.browser.icons.loader.MemoryIconLoader.LoaderMemoryCache
import mozilla.components.browser.icons.preparer.MemoryIconPreparer
import mozilla.components.browser.icons.processor.MemoryIconProcessor.ProcessorMemoryCache

private const val MAXIMUM_CACHE_URLS = 1000

// Use a better mechanism for determining the cache size:
// https://github.com/mozilla-mobile/android-components/issues/2764
private const val MAXIMUM_CACHE_BITMAP_BYTES = 1024 * 1024 * 25 // 25 MB

class IconMemoryCache : ProcessorMemoryCache, LoaderMemoryCache, MemoryIconPreparer.PreparerMemoryCache {
    private val iconResourcesCache = LruCache<String, List<IconRequest.Resource>>(MAXIMUM_CACHE_URLS)
    private val iconBitmapCache = object : LruCache<String, Bitmap>(MAXIMUM_CACHE_BITMAP_BYTES) {
        override fun sizeOf(key: String, value: Bitmap): Int {
            return value.byteCount
        }
    }

    override fun getResources(request: IconRequest): List<IconRequest.Resource> {
        return iconResourcesCache[request.url] ?: emptyList()
    }

    override fun getBitmap(request: IconRequest, resource: IconRequest.Resource): Bitmap? {
        return iconBitmapCache[resource.url]
    }

    override fun put(request: IconRequest, resource: IconRequest.Resource, icon: Icon) {
        if (icon.source.shouldCacheInMemory) {
            iconBitmapCache.put(resource.url, icon.bitmap)
        }

        if (request.resources.isNotEmpty()) {
            iconResourcesCache.put(request.url, request.resources)
        }
    }

    internal fun clear() {
        iconResourcesCache.evictAll()
        iconBitmapCache.evictAll()
    }
}

private val Icon.Source.shouldCacheInMemory: Boolean
    get() {
        return when (this) {
            Icon.Source.DOWNLOAD -> true
            Icon.Source.INLINE -> true
            Icon.Source.GENERATOR -> false
            Icon.Source.MEMORY -> false
            Icon.Source.DISK -> true
        }
    }
