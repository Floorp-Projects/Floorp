/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.io.IOException

/**
 * Manages the migration of legacy wallpapers to the new paths
 *
 * @property storageRootDirectory The top level app-local storage directory.
 */
class LegacyWallpaperMigration(
    private val storageRootDirectory: File,
) {
    /**
     * Migrate the legacy wallpaper to the new path and delete the remaining legacy files.
     *
     * @param wallpaperName Name of the wallpaper to be migrated.
     */
    suspend fun migrateLegacyWallpaper(
        wallpaperName: String,
    ) = withContext(Dispatchers.IO) {
        val legacyPortraitFile =
            File(storageRootDirectory, "wallpapers/portrait/light/$wallpaperName.png")
        val legacyLandscapeFile =
            File(storageRootDirectory, "wallpapers/landscape/light/$wallpaperName.png")
        // If any of portrait or landscape files of the wallpaper are missing, then we shouldn't
        // migrate it
        if (!legacyLandscapeFile.exists() || !legacyPortraitFile.exists()) {
            return@withContext
        }
        // Directory where the legacy wallpaper files should be migrated
        val targetDirectory = "wallpapers/${wallpaperName.lowercase()}"

        try {
            // Use the portrait file as thumbnail
            legacyPortraitFile.copyTo(
                File(
                    storageRootDirectory,
                    "$targetDirectory/thumbnail.png",
                ),
            )
            // Copy the portrait file
            legacyPortraitFile.copyTo(
                File(
                    storageRootDirectory,
                    "$targetDirectory/portrait.png",
                ),
            )
            // Copy the landscape file
            legacyLandscapeFile.copyTo(
                File(
                    storageRootDirectory,
                    "$targetDirectory/landscape.png",
                ),
            )
        } catch (e: IOException) {
            Logger.error("Failed to migrate legacy wallpaper", e)
        }

        // Delete the remaining legacy files
        File(storageRootDirectory, "wallpapers/portrait").deleteRecursively()
        File(storageRootDirectory, "wallpapers/landscape").deleteRecursively()
    }
}
