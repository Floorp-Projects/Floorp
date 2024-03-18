/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import android.util.Base64
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * An [IconLoader] implementation that will base64 decode the image bytes from a data:image uri.
 */
class DataUriIconLoader : IconLoader {
    override fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): IconLoader.Result {
        if (!resource.url.startsWith("data:image/")) {
            return IconLoader.Result.NoResult
        }

        val offset = resource.url.indexOf(',') + 1
        if (offset == 0) {
            return IconLoader.Result.NoResult
        }

        @Suppress("TooGenericExceptionCaught")
        return try {
            IconLoader.Result.BytesResult(
                Base64.decode(resource.url.substring(offset), Base64.DEFAULT),
                Icon.Source.INLINE,
            )
        } catch (e: Exception) {
            IconLoader.Result.NoResult
        }
    }
}
