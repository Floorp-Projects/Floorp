/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import org.mozilla.fenix.onboarding.WallpaperOnboardingDialogFragment.Companion.CLASSIC_WALLPAPERS_COUNT
import org.mozilla.fenix.onboarding.WallpaperOnboardingDialogFragment.Companion.SEASONAL_WALLPAPERS_COUNT
import org.mozilla.fenix.onboarding.WallpaperOnboardingDialogFragment.Companion.THUMBNAILS_SELECTION_COUNT
import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * The extension function to group wallpapers into the appropriate collections for display.
 **/
fun List<Wallpaper>.groupByDisplayableCollection(): Map<Wallpaper.Collection, List<Wallpaper>> =
    groupBy {
        it.collection
    }.filter {
        it.key.name != "default"
    }.map {
        val wallpapers = it.value.filter { wallpaper ->
            wallpaper.thumbnailFileState == Wallpaper.ImageFileState.Downloaded
        }
        if (it.key.name == Wallpaper.classicFirefoxCollectionName) {
            it.key to listOf(Wallpaper.Default) + wallpapers
        } else {
            it.key to wallpapers
        }
    }.toMap().let { result ->
        // Ensure the default is shown in the classic firefox collection even if those wallpapers are
        // missing
        if (result.keys.any { it.name == Wallpaper.classicFirefoxCollectionName }) {
            result
        } else {
            result.plus(Wallpaper.ClassicFirefoxCollection to listOf(Wallpaper.Default))
        }
    }

/**
 * Returns a list of wallpapers to display in the wallpaper onboarding.
 *
 * The ideal scenario is to return a list of wallpaper in the following order: 1 default, 3 seasonal and
 * 2 classic wallpapers, but in case where there are less than 3 seasonal wallpapers, the remaining
 * wallpapers are filled by classic wallpapers. If we have less than 6 wallpapers, return all the available
 * seasonal and classic wallpapers.
 */
fun List<Wallpaper>.getWallpapersForOnboarding(): List<Wallpaper> {
    val result = mutableListOf(Wallpaper.Default)
    val classicWallpapers = mutableListOf<Wallpaper>()
    val seasonalWallpapers = mutableListOf<Wallpaper>()

    for (wallpaper in this) {
        if (wallpaper == Wallpaper.Default) continue

        if (wallpaper.collection.name == Wallpaper.classicFirefoxCollectionName) {
            classicWallpapers.add(wallpaper)
        } else {
            seasonalWallpapers.add(wallpaper)
        }
    }

    if (seasonalWallpapers.size < SEASONAL_WALLPAPERS_COUNT) {
        result.addAll(seasonalWallpapers)
        result.addAll(classicWallpapers.take((THUMBNAILS_SELECTION_COUNT - 1) - seasonalWallpapers.size))
    } else if (classicWallpapers.size < CLASSIC_WALLPAPERS_COUNT) {
        result.addAll(seasonalWallpapers.take((THUMBNAILS_SELECTION_COUNT - 1) - classicWallpapers.size))
        result.addAll(classicWallpapers)
    } else {
        result.addAll(seasonalWallpapers.take(SEASONAL_WALLPAPERS_COUNT))
        result.addAll(classicWallpapers.take(CLASSIC_WALLPAPERS_COUNT))
    }

    return result
}
