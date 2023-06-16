/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import org.mozilla.fenix.utils.Settings
import java.util.Date

/**
 * Type that represents wallpapers.
 *
 * @property name The name of the wallpaper.
 * @property collection The name of the collection the wallpaper belongs to.
 * is not restricted.
 * @property textColor The 8 digit hex code color that should be used for text overlaying the wallpaper.
 * @property cardColorLight The 8 digit hex code color that should be used for cards overlaying the wallpaper
 * when the user's theme is set to Light.
 * @property cardColorDark The 8 digit hex code color that should be used for cards overlaying the wallpaper
 * when the user's theme is set to Dark.
 */
data class Wallpaper(
    val name: String,
    val collection: Collection,
    val textColor: Long?,
    val cardColorLight: Long?,
    val cardColorDark: Long?,
    val thumbnailFileState: ImageFileState,
    val assetsFileState: ImageFileState,
) {
    /**
     * Type that represents a collection that a [Wallpaper] belongs to.
     *
     * @property name The name of the collection the wallpaper belongs to.
     * @property learnMoreUrl The URL that can be visited to learn more about a collection, if any.
     * @property availableLocales The locales that this wallpaper is restricted to. If null, the wallpaper
     * is not restricted.
     * @property startDate The date the wallpaper becomes available in a promotion. If null, it is available
     * from any date.
     * @property endDate The date the wallpaper stops being available in a promotion. If null,
     * the wallpaper will be available to any date.
     */
    data class Collection(
        val name: String,
        val heading: String?,
        val description: String?,
        val learnMoreUrl: String?,
        val availableLocales: List<String>?,
        val startDate: Date?,
        val endDate: Date?,
    )

    companion object {
        const val amethystName = "amethyst"
        const val ceruleanName = "cerulean"
        const val sunriseName = "sunrise"
        const val twilightHillsName = "twilight-hills"
        const val beachVibeName = "beach-vibe"
        const val firefoxCollectionName = "firefox"
        const val defaultName = "default"

        /*
         * Note: this collection could get out of sync with the version of it generated when fetching
         * remote metadata. It is included mostly for convenience, but use with utmost care until
         * we find a better way of handling the edge cases around this collection. It is generally
         * safer to do comparison directly with the collection name.
         */
        const val classicFirefoxCollectionName = "classic-firefox"
        val ClassicFirefoxCollection = Collection(
            name = classicFirefoxCollectionName,
            heading = null,
            description = null,
            learnMoreUrl = null,
            availableLocales = null,
            startDate = null,
            endDate = null,
        )
        val DefaultCollection = Collection(
            name = defaultName,
            heading = null,
            description = null,
            learnMoreUrl = null,
            availableLocales = null,
            startDate = null,
            endDate = null,
        )
        val Default = Wallpaper(
            name = defaultName,
            collection = DefaultCollection,
            textColor = null,
            cardColorLight = null,
            cardColorDark = null,
            thumbnailFileState = ImageFileState.Downloaded,
            assetsFileState = ImageFileState.Downloaded,
        )

        /**
         * Defines the standard path at which a wallpaper resource is kept on disk.
         *
         * @param orientation One of landscape/portrait.
         * @param theme One of dark/light.
         * @param name The name of the wallpaper.
         */
        fun legacyGetLocalPath(orientation: String, theme: String, name: String): String =
            "wallpapers/$orientation/$theme/$name.png"

        /**
         * Defines the standard path at which a wallpaper resource is kept on disk.
         *
         * @param type The type of image that should be retrieved.
         * @param name The name of the wallpaper.
         */
        fun getLocalPath(name: String, type: ImageType) = "wallpapers/$name/${type.lowercase()}.png"

        /**
         * Generate a wallpaper from metadata cached in Settings.
         *
         * @param settings The local cache.
         */
        @Suppress("ComplexCondition")
        fun getCurrentWallpaperFromSettings(settings: Settings): Wallpaper? {
            val name = settings.currentWallpaperName
            val textColor = settings.currentWallpaperTextColor
            val cardColorLight = settings.currentWallpaperCardColorLight
            val cardColorDark = settings.currentWallpaperCardColorDark
            return if (name.isNotEmpty() && textColor != 0L && cardColorLight != 0L && cardColorDark != 0L) {
                Wallpaper(
                    name = name,
                    textColor = textColor,
                    cardColorLight = cardColorLight,
                    cardColorDark = cardColorDark,
                    collection = DefaultCollection,
                    thumbnailFileState = ImageFileState.Downloaded,
                    assetsFileState = ImageFileState.Downloaded,
                )
            } else {
                null
            }
        }

        /**
         * Check if a wallpaper name matches the default. Considers empty strings to be default
         * since that likely means a wallpaper has never been set. The "none" case here is to deal
         * with a legacy case where the default wallpaper used to be Wallpaper.NONE. See
         * commit 7a44412, Wallpaper.NONE and Settings.currentWallpaper (legacy name) for context.
         *
         * @param name The name to check.
         */
        fun nameIsDefault(name: String): Boolean =
            name.isEmpty() || name == defaultName || name.lowercase() == "none"
    }

    /**
     * Defines various image asset types that can be downloaded for each wallpaper.
     */
    enum class ImageType {
        Portrait,
        Landscape,
        Thumbnail,
        ;

        /**
         * Get a lowercase string representation of the [ImageType.name] for use in path segments.
         */
        fun lowercase(): String = this.name.lowercase()
    }

    /**
     * Defines the download state of wallpaper asset.
     */
    enum class ImageFileState {
        Unavailable,
        Downloading,
        Downloaded,
        Error,
    }
}
