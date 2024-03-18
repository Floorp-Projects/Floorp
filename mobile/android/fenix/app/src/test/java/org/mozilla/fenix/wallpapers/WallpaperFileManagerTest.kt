/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import org.mozilla.fenix.utils.Settings
import java.io.File

class WallpaperFileManagerTest {
    @Rule
    @JvmField
    val tempFolder = TemporaryFolder()
    private lateinit var wallpapersFolder: File

    private val dispatcher = UnconfinedTestDispatcher()

    private lateinit var fileManager: WallpaperFileManager

    private lateinit var settings: Settings

    @Before
    fun setup() {
        wallpapersFolder = File(tempFolder.root, "wallpapers")
        fileManager = WallpaperFileManager(
            storageRootDirectory = tempFolder.root,
            coroutineDispatcher = dispatcher,
        )
        settings = mockk {
            every { currentWallpaperName } returns wallpaperName
            every { currentWallpaperTextColor } returns 0L
            every { currentWallpaperCardColorLight } returns 0L
            every { currentWallpaperCardColorDark } returns 0L
        }
    }

    @Test
    fun `GIVEN wallpaper directory exists WHEN looked up THEN wallpaper created with correct name`() = runTest {
        createAllFiles(wallpaperName)

        val result = fileManager.lookupExpiredWallpaper(settings)

        val expected = generateWallpaper(name = wallpaperName)
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN portrait file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        File(wallpapersFolder, "$wallpaperName/landscape.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/thumbnail.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(settings)

        assertEquals(null, result)
    }

    @Test
    fun `GIVEN landscape file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        File(wallpapersFolder, "$wallpaperName/portrait.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/thumbnail.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(settings)

        assertEquals(null, result)
    }

    @Test
    fun `GIVEN thumbnail file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        File(wallpapersFolder, "$wallpaperName/portrait.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/landscape.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(settings)

        assertEquals(null, result)
    }

    @Test
    fun `WHEN cleaned THEN current wallpaper and available wallpapers kept`() = runTest {
        val currentName = "current"
        val currentWallpaper = generateWallpaper(name = currentName)
        val availableName = "available"
        val available = generateWallpaper(name = availableName)
        val unavailableName = "unavailable"
        createAllFiles(currentName)
        createAllFiles(availableName)
        createAllFiles(unavailableName)

        fileManager.clean(currentWallpaper, listOf(available))

        assertTrue(getAllFiles(currentName).all { it.exists() })
        assertTrue(getAllFiles(availableName).all { it.exists() })
        assertTrue(getAllFiles(unavailableName).none { it.exists() })
    }

    @Test
    fun `WHEN both wallpaper assets exist THEN the file lookup will succeed`() = runTest {
        val wallpaper = generateWallpaper("name")
        createAllFiles(wallpaper.name)

        val result = fileManager.wallpaperImagesExist(wallpaper)

        assertTrue(result)
    }

    @Test
    fun `WHEN at least one wallpaper asset does not exist THEN the file lookup will fail`() = runTest {
        val wallpaper = generateWallpaper("name")
        val allFiles = getAllFiles(wallpaper.name)
        (0 until (allFiles.size - 1)).forEach {
            allFiles[it].mkdirs()
            allFiles[it].createNewFile()
        }

        val result = fileManager.wallpaperImagesExist(wallpaper)

        assertFalse(result)
    }

    private fun createAllFiles(name: String) {
        for (file in getAllFiles(name)) {
            file.mkdirs()
            file.createNewFile()
        }
    }

    private fun getAllFiles(name: String): List<File> {
        val folder = File(wallpapersFolder, name)
        return listOf(
            folder,
            File(folder, "portrait.png"),
            File(folder, "landscape.png"),
            File(folder, "thumbnail.png"),
        )
    }

    private fun generateWallpaper(name: String) = Wallpaper(
        name = name,
        textColor = 0L,
        cardColorLight = 0L,
        cardColorDark = 0L,
        thumbnailFileState = Wallpaper.ImageFileState.Downloaded,
        assetsFileState = Wallpaper.ImageFileState.Downloaded,
        collection = Wallpaper.DefaultCollection,
    )

    private companion object {
        const val wallpaperName = "name"
    }
}
