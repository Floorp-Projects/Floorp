/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images.compose.loader

import androidx.compose.ui.graphics.painter.BitmapPainter

/**
 * The state an [ImageLoaderScope] is in.
 */
sealed class ImageLoaderState {
    /**
     * The [ImageLoader] is currently loading the image.
     */
    object Loading : ImageLoaderState()

    /**
     * The [ImageLoader] succesfully loaded the image.
     */
    data class Image(
        val painter: BitmapPainter,
    ) : ImageLoaderState()

    /**
     * Loading the image failed.
     */
    object Failed : ImageLoaderState()
}
