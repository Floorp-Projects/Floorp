/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images

import androidx.annotation.DimenRes

/**
 * A request to load an image.
 *
 * @property id The id of the image to retrieve.
 * @property size The preferred size of the image that should be loaded.
 */
data class ImageRequest(
    val id: String,
    val size: Size = Size.DEFAULT
) {

    /**
     * Supported sizes.
     */
    enum class Size(@DimenRes val dimen: Int) {
        DEFAULT(R.dimen.mozac_support_images_size_default)
    }
}
