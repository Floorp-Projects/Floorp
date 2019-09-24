/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.request

class LoadRequestMetadata(
    val url: String,
    options: Array<LoadRequestOption> = emptyArray()
) {
    private val optionMask: Long
    init {
        optionMask = options.fold(0L) { acc, it ->
            acc or it.mask
        }
    }

    fun isSet(option: LoadRequestOption) = optionMask and option.mask > 0

    companion object {
        val blank = LoadRequestMetadata("about:blank")
    }
}

/**
 * Simple enum class for defining the set of characteristics of a [LoadRequest].
 *
 * Facilities for combining these and testing the resulting bit mask also exist as operators.
 *
 * This should be generalized, but it's not clear if it will be useful enough to go into [kotlin.support].
 */
enum class LoadRequestOption constructor(internal val mask: Long) {
    NONE(0L),
    REDIRECT(1L shl 0),
    WEB_CONTENT(1L shl 1);
}
