/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.manifest

/**
 * Represents dimensions for an image.
 * Corresponds to values of the "sizes" HTML attribute.
 *
 * @property width Width of the image.
 * @property height Height of the image.
 */
data class Size(
    val width: Int,
    val height: Int
) {
    override fun toString() = if (this == ANY) "any" else "${width}x$height"

    companion object {
        /**
         * Represents the "any" size.
         */
        val ANY = Size(Int.MAX_VALUE, Int.MAX_VALUE)

        /**
         * Parse a value from an HTML sizes attribute (512x512, 16x16, etc).
         * Returns null if the value was invalid.
         */
        fun parse(raw: String): Size? {
            if (raw == "any") return ANY

            val size = raw.split("x")
            if (size.size != 2) return null

            return try {
                val width = size[0].toInt()
                val height = size[1].toInt()
                Size(width, height)
            } catch (e: NumberFormatException) {
                null
            }
        }
    }
}
