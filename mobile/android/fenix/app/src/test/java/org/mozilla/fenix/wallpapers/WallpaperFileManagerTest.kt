/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.test.TestCoroutineDispatcher
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class WallpaperFileManagerTest {
    @Rule
    @JvmField
    val tempFolder = TemporaryFolder()
    private lateinit var portraitLightFolder: File
    private lateinit var portraitDarkFolder: File
    private lateinit var landscapeLightFolder: File
    private lateinit var landscapeDarkFolder: File

    private val dispatcher = TestCoroutineDispatcher()

    private lateinit var fileManager: WallpaperFileManager

    @Before
    fun setup() {
        portraitLightFolder = tempFolder.newFolder("wallpapers", "portrait", "light")
        portraitDarkFolder = tempFolder.newFolder("wallpapers", "portrait", "dark")
        landscapeLightFolder = tempFolder.newFolder("wallpapers", "landscape", "light")
        landscapeDarkFolder = tempFolder.newFolder("wallpapers", "landscape", "dark")
        fileManager = WallpaperFileManager(
            rootDirectory = tempFolder.root,
            coroutineDispatcher = dispatcher,
        )
    }

    @Test
    fun `GIVEN files exist in all directories WHEN expired wallpaper looked up THEN expired wallpaper returned`() {
        val wallpaperName = "name"
        createAllFiles(wallpaperName)

        val expected = Wallpaper.Expired(name = wallpaperName)
        assertEquals(expected, fileManager.lookupExpiredWallpaper(wallpaperName))
    }

    @Test
    fun `GIVEN any missing file in directories WHEN expired wallpaper looked up THEN null returned`() {
        val wallpaperName = "name"
        File(landscapeLightFolder, "$wallpaperName.png").createNewFile()
        File(landscapeDarkFolder, "$wallpaperName.png").createNewFile()

        assertEquals(null, fileManager.lookupExpiredWallpaper(wallpaperName))
    }

    @Test
    fun `WHEN cleaned THEN current wallpaper and available wallpapers kept`() {
        val currentName = "current"
        val currentWallpaper = Wallpaper.Expired(currentName)
        val availableName = "available"
        val available = Wallpaper.Remote.House(name = availableName)
        val unavailableName = "unavailable"
        createAllFiles(currentName)
        createAllFiles(availableName)
        createAllFiles(unavailableName)

        fileManager.clean(currentWallpaper, listOf(available))

        assertTrue(getAllFiles(currentName).all { it.exists() })
        assertTrue(getAllFiles(availableName).all { it.exists() })
        assertTrue(getAllFiles(unavailableName).none { it.exists() })
    }

    private fun createAllFiles(name: String) {
        for (file in getAllFiles(name)) {
            file.createNewFile()
        }
    }

    private fun getAllFiles(name: String): List<File> {
        return listOf(
            File(portraitLightFolder, "$name.png"),
            File(portraitDarkFolder, "$name.png"),
            File(landscapeLightFolder, "$name.png"),
            File(landscapeDarkFolder, "$name.png"),
        )
    }
}
