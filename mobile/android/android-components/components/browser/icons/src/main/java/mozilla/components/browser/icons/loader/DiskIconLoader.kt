/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * [IconLoader] implementation that loads icons from a disk cache.
 */
class DiskIconLoader(
    private val cache: LoaderDiskCache,
) : IconLoader {
    interface LoaderDiskCache {
        fun getIconData(context: Context, resource: IconRequest.Resource): ByteArray?
    }

    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        return cache.getIconData(context, resource)?.let { data ->
            IconLoader.Result.BytesResult(data, Icon.Source.DISK)
        } ?: IconLoader.Result.NoResult
    }
}
