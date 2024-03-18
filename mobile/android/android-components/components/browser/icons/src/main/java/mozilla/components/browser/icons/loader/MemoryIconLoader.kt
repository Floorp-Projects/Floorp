/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * An [IconLoader] implementation that loads icons from an in-memory cache.
 */
class MemoryIconLoader(
    private val cache: LoaderMemoryCache,
) : IconLoader {
    interface LoaderMemoryCache {
        fun getBitmap(request: IconRequest, resource: IconRequest.Resource): Bitmap?
    }

    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        return cache.getBitmap(request, resource)?.let { bitmap ->
            IconLoader.Result.BitmapResult(bitmap, Icon.Source.MEMORY)
        } ?: IconLoader.Result.NoResult
    }
}
