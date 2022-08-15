/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import java.util.Date

/**
 * Type that represents wallpapers.
 *
 * @property name The name of the wallpaper.
 * @property collectionName The name of the collection the wallpaper belongs to.
 * @property availableLocales The locales that this wallpaper is restricted to. If null, the wallpaper
 * is not restricted.
 * @property startDate The date the wallpaper becomes available in a promotion. If null, it is available
 * from any date.
 * @property endDate The date the wallpaper stops being available in a promotion. If null,
 * the wallpaper will be available to any date.
 */
data class Wallpaper(
    val name: String,
    val collectionName: String,
    val availableLocales: List<String>?,
    val startDate: Date?,
    val endDate: Date?
) {
    companion object {
        const val amethystName = "amethyst"
        const val ceruleanName = "cerulean"
        const val sunriseName = "sunrise"
        const val twilightHillsName = "twilight-hills"
        const val beachVibeName = "beach-vibe"
        const val firefoxCollectionName = "firefox"
        const val defaultName = "default"
        val Default = Wallpaper(
            name = defaultName,
            collectionName = defaultName,
            availableLocales = null,
            startDate = null,
            endDate = null,
        )

        /**
         * Defines the standard path at which a wallpaper resource is kept on disk.
         *
         * @param orientation One of landscape/portrait.
         * @param theme One of dark/light.
         * @param name The name of the wallpaper.
         */
        fun getBaseLocalPath(orientation: String, theme: String, name: String): String =
            "wallpapers/$orientation/$theme/$name.png"
    }
}
