/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.processor

import android.content.Context
import mozilla.components.browser.icons.DesiredSize
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.Icon.Source
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

    override fun process(
        context: Context,
        request: IconRequest,
        resource: IconRequest.Resource?,
        icon: Icon,
        desiredSize: DesiredSize
    ): Icon {
        if (resource != null && icon.shouldCacheOnDisk && !request.isPrivate) {
            cache.put(context, request, resource, icon)
        }

        return icon
    }
}

private val Icon.shouldCacheOnDisk: Boolean
    get() = when (source) {
        Source.DOWNLOAD, Source.INLINE -> true
        Source.GENERATOR, Source.MEMORY, Source.DISK -> false
    }
