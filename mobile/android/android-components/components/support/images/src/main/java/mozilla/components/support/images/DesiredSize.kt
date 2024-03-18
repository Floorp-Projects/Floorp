/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.images

import androidx.annotation.Px

/**
 * Represents the desired size of an images to be loaded.
 *
 * @property targetSize The size the image will be displayed at, in pixels.
 * @property minSize The minimum size of an image before it will be thrown out, in pixels.
 * @property maxSize The maximum size of an image before it will be thrown out, in pixels.
 * Extremely large images are suspicious and should be ignored.
 * @property maxScaleFactor The factor that the image can be scaled up before being thrown out.
 * A lower scale factor results in less pixelation.
 */
data class DesiredSize(
    @Px val targetSize: Int,
    @Px val minSize: Int,
    @Px val maxSize: Int,
    val maxScaleFactor: Float,
)
