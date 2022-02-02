/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.content.Context
import android.content.res.Configuration
import org.mozilla.fenix.R

/**
 * A class that represents an available wallpaper and its state.
 * @property name Indicates the name of this wallpaper.
 * @property portraitPath A file path for the portrait version of this wallpaper.
 * @property landscapePath A file path for the landscape version of this wallpaper.
 * @property isDark Indicates if the most predominant color on the wallpaper is dark.
 * @property themeCollection The theme collection this wallpaper belongs to.
 */
data class Wallpaper(
    val name: String,
    val themeCollection: WallpaperThemeCollection,
)

/**
 * A type hierarchy representing the different theme collections [Wallpaper]s belong to.
 */
enum class WallpaperThemeCollection(val origin: WallpaperOrigin) {
    NONE(WallpaperOrigin.LOCAL),
    FIREFOX(WallpaperOrigin.LOCAL),
    FOCUS(WallpaperOrigin.REMOTE),
}

/**
 * The parent directory name of a wallpaper. Since wallpapers that are [WallpaperOrigin.LOCAL] are
 * stored in drawables, this extension is not applicable to them.
 */
val WallpaperThemeCollection.directoryName: String get() = when (this) {
    WallpaperThemeCollection.NONE,
    WallpaperThemeCollection.FIREFOX -> ""
    WallpaperThemeCollection.FOCUS -> "focus"
}

/**
 * Types defining whether a [Wallpaper] is delivered through a remote source or is included locally
 * in the APK.
 */
enum class WallpaperOrigin {
    LOCAL,
    REMOTE,
}

val Wallpaper.drawableId: Int get() = when (name) {
    "amethyst" -> R.drawable.amethyst
    "cerulean" -> R.drawable.cerulean
    "sunrise" -> R.drawable.sunrise
    else -> -1
}

/**
 * Get the expected local path on disk for a wallpaper. This will differ depending
 * on orientation and app theme.
 */
fun Wallpaper.getLocalPathFromContext(context: Context): String {
    val orientation = if (context.isLandscape()) "landscape" else "portrait"
    val theme = if (context.isDark()) "dark" else "light"
    return getLocalPath(orientation, theme)
}

/**
 * Get the expected local path on disk for a wallpaper if orientation and app theme are known.
 */
fun Wallpaper.getLocalPath(orientation: String, theme: String): String =
    "$orientation/$theme/${themeCollection.directoryName}/$name.png"

private fun Context.isLandscape(): Boolean {
    return resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
}

private fun Context.isDark(): Boolean {
    return resources.configuration.uiMode and
        Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
}
