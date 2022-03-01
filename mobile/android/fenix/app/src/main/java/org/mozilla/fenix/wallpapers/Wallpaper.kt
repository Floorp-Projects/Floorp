/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import androidx.annotation.DrawableRes
import java.util.Calendar
import java.util.Date

/**
 * Type hierarchy defining the various wallpapers that are available as home screen backgrounds.
 * @property name The name of the wallpaper.
 */
sealed class Wallpaper {
    abstract val name: String

    /**
     * The default wallpaper. This uses the standard color resource to as a background, instead of
     * loading a bitmap.
     */
    object Default : Wallpaper() {
        override val name = "default"
    }

    /**
     * If a user had previously selected a wallpaper, they are allowed to retain it even if
     * the wallpaper is otherwise expired. This type exists as a wrapper around that current
     * wallpaper.
     */
    data class Expired(override val name: String) : Wallpaper()

    /**
     * Wallpapers that are included directly in the shipped APK.
     *
     * @property drawableId The drawable bitmap used as the background.
     */
    sealed class Local : Wallpaper() {
        abstract val drawableId: Int
        data class Firefox(override val name: String, @DrawableRes override val drawableId: Int) : Local()
    }

    /**
     * Wallpapers that need to be fetched from a network resource.
     *
     * @property expirationDate Optional date at which this wallpaper should no longer be available.
     */
    sealed class Remote : Wallpaper() {
        abstract val expirationDate: Date?
        abstract val remoteParentDirName: String
        @Suppress("MagicNumber")
        data class House(
            override val name: String,
            override val expirationDate: Date? = Calendar.getInstance().run {
                set(2022, Calendar.APRIL, 30)
                time
            }
        ) : Remote() {
            override val remoteParentDirName: String = "house"
        }
    }

    companion object {
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
