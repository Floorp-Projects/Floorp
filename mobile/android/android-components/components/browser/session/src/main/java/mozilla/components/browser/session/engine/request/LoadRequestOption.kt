/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.session.engine.request

/**
 * Simple enum class for defining the set of characteristics of a [LoadRequest].
 *
 * Facilities for combining these and testing the resulting bit mask also exist as operators.
 *
 * This should be generalized, but it's not clear if it will be useful enough to go into [kotlin.support].
 */
enum class LoadRequestOption constructor(val mask: Int) {
    NONE(0),
    REDIRECT(1 shl 0),
    WEB_CONTENT(1 shl 1);

    fun toMask() = mask
}

infix operator fun LoadRequestOption.plus(other: LoadRequestOption) = this.mask or other.mask
infix operator fun Int.plus(other: LoadRequestOption) = this or other.mask
infix fun Int.isSet(option: LoadRequestOption) = this and option.mask > 0
