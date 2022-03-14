/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File

class WallpaperFileManager(
    private val rootDirectory: File,
    coroutineDispatcher: CoroutineDispatcher = Dispatchers.IO
) {
    private val scope = CoroutineScope(coroutineDispatcher)
    private val portraitDirectory = File(rootDirectory, "wallpapers/portrait")
    private val landscapeDirectory = File(rootDirectory, "wallpapers/landscape")

    /**
     * Lookup all the files for a wallpaper name. This lookup will fail if there are not
     * files for each of the following orientation and theme combinations:
     * light/portrait - light/landscape - dark/portrait - dark/landscape
     */
    fun lookupExpiredWallpaper(name: String): Wallpaper.Expired? {
        return if (getAllLocalWallpaperPaths(name).all { File(rootDirectory, it).exists() }) {
            Wallpaper.Expired(name)
        } else null
    }

    private fun getAllLocalWallpaperPaths(name: String): List<String> =
        listOf("landscape", "portrait").flatMap { orientation ->
            listOf("light", "dark").map { theme ->
                Wallpaper.getBaseLocalPath(orientation, theme, name)
            }
        }

    /**
     * Remove all wallpapers that are not the [currentWallpaper] or in [availableWallpapers].
     */
    fun clean(currentWallpaper: Wallpaper, availableWallpapers: List<Wallpaper.Remote>) {
        scope.launch {
            val wallpapersToKeep = (listOf(currentWallpaper) + availableWallpapers).map { it.name }
            cleanChildren(portraitDirectory, wallpapersToKeep)
            cleanChildren(landscapeDirectory, wallpapersToKeep)
        }
    }

    private fun cleanChildren(dir: File, wallpapersToKeep: List<String>) {
        for (file in dir.walkTopDown()) {
            if (file.isDirectory || file.nameWithoutExtension in wallpapersToKeep) continue
            file.delete()
        }
    }
}
