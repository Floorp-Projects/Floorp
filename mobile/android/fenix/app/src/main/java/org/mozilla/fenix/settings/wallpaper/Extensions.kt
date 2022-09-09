/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * The extension function to group wallpapers according to their name.
 **/
fun List<Wallpaper>.groupByDisplayableCollection(): Map<Wallpaper.Collection, List<Wallpaper>> = groupBy {
    it.collection
}.filter {
    it.key.name != "default"
}.map {
    val wallpapers = it.value.filter { wallpaper ->
        wallpaper.thumbnailFileState == Wallpaper.ImageFileState.Downloaded
    }
    if (it.key.name == "classic-firefox") {
        it.key to listOf(Wallpaper.Default) + wallpapers
    } else {
        it.key to wallpapers
    }
}.toMap().takeIf {
    it.isNotEmpty()
} ?: mapOf(Wallpaper.DefaultCollection to listOf(Wallpaper.Default))
