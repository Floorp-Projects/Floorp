/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.loader

import android.content.Context
import android.graphics.Bitmap
import mozilla.components.browser.icons.Icon
import mozilla.components.browser.icons.IconRequest

/**
 * A loader that can load an icon from an [IconRequest.Resource].
 */
interface IconLoader {
    /**
     * Tries to load the [IconRequest.Resource] for the given [IconRequest].
     */
    fun load(context: Context, request: IconRequest, resource: IconRequest.Resource): Result

    sealed class Result {
        object NoResult : Result()

        class BitmapResult(
            val bitmap: Bitmap,
            val source: Icon.Source,
        ) : Result()

        class BytesResult(
            val bytes: ByteArray,
            val source: Icon.Source,
        ) : Result()
    }
}
