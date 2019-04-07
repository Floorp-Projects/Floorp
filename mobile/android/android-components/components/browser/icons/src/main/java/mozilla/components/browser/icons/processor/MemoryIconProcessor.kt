/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * An [IconProcessor] implementation that saves icons in the in-memory cache.
 */
class MemoryIconProcessor(
    private val cache: ProcessorMemoryCache
) : IconProcessor {
    interface ProcessorMemoryCache {
        fun put(request: IconRequest, resource: IconRequest.Resource, icon: Icon)
    }

    override fun process(request: IconRequest, resource: IconRequest.Resource?, icon: Icon): Icon {
        if (resource != null && icon.shouldCacheInMemory) {
            cache.put(request, resource, icon)
        }

        return icon
    }
}

private val Icon.shouldCacheInMemory: Boolean
    get() = when (source) {
        Icon.Source.GENERATOR -> false
        Icon.Source.DOWNLOAD -> true
        Icon.Source.INLINE -> true
        Icon.Source.MEMORY -> false
    }
