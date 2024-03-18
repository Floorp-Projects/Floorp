/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.base.images

import androidx.annotation.Px

/**
 * A request to save an image. This is an alias for the id of the image.
 *
 * @property id The id of the image to save
 * @property isPrivate Whether the image is related to a private tab.
 */
data class ImageSaveRequest(val id: String, val isPrivate: Boolean)

/**
 * A request to load an image.
 *
 * @property id The id of the image to retrieve.
 * @property size The preferred size of the image that should be loaded in pixels.
 * @property isPrivate Whether the image is related to a private tab.
 */
data class ImageLoadRequest(
    val id: String,
    @Px val size: Int,
    val isPrivate: Boolean,
)
