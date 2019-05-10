/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * [IconProcessor] implementation that saves icons in the disk cache.
 */
class DiskIconProcessor(
    private val cache: ProcessorDiskCache
) : IconProcessor {
    interface ProcessorDiskCache {
        fun put(context: Context, request: IconRequest, resource: IconRequest.Resource, icon: Icon)
    }

    override fun process(context: Context, request: IconRequest, resource: IconRequest.Resource?, icon: Icon): Icon {
        if (resource != null && icon.shouldCacheOnDisk) {
            cache.put(context, request, resource, icon)
        }

        return icon
    }
}

private val Icon.shouldCacheOnDisk: Boolean
    get() = when (source) {
        Icon.Source.GENERATOR -> false
        Icon.Source.DOWNLOAD -> true
        Icon.Source.INLINE -> true
        Icon.Source.MEMORY -> false
        Icon.Source.DISK -> false
    }
