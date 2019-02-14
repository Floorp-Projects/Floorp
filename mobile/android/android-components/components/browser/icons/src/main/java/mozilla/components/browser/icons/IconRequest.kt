/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons

/**
 * A request to load an [Icon].
 *
 * @property url The URL of the website an icon should be loaded for.
 * @property size The preferred size of the icon that should be loaded.
 */
data class IconRequest(
    val url: String,
    val size: Size = Size.DEFAULT
) {
    /**
     * Supported sizes.
     *
     * We are trying to limit the supported sizes in order to optimize our caching strategy.
     */
    @Suppress("MagicNumber")
    enum class Size(
        val value: Int
    ) {
        DEFAULT(32),
        LAUNCHER(48)
    }
}
