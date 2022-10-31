/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.images

import androidx.annotation.Px

/**
 * A request to save an image. This is an alias for the id of the image.
 */
typealias ImageSaveRequest = String

/**
 * A request to load an image.
 *
 * @property id The id of the image to retrieve.
 * @property size The preferred size of the image that should be loaded in pixels.
 */
data class ImageLoadRequest(
    val id: String,
    @Px val size: Int,
)
