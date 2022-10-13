/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.Icon.Source
import mozilla.components.browser.icons.IconRequest
import mozilla.components.support.images.DesiredSize

/**
 * [IconProcessor] implementation that saves icons in the disk cache.
 */
class DiskIconProcessor(
    private val cache: ProcessorDiskCache,
) : IconProcessor {
    interface ProcessorDiskCache {
        /**
         * Saves icon resources to cache.
         * */
        fun putResources(context: Context, request: IconRequest)

        /**
         * Saves icon bitmap to cache.
         * */
        fun putIcon(context: Context, resource: IconRequest.Resource, icon: Icon)
    }

    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize,
    ): Icon {
        if (resource != null && !request.isPrivate) {
            cache.putResources(context, request)
            if (icon.shouldCacheOnDisk) {
                cache.putIcon(context, resource, icon)
            }
        }
        return icon
    }
}

private val Icon.shouldCacheOnDisk: Boolean
    get() = when (source) {
        Source.DOWNLOAD, Source.INLINE -> true
        Source.GENERATOR, Source.MEMORY, Source.DISK -> false
    }
