package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.runTest
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
    private lateinit var wallpapersFolder: File

    private val dispatcher = UnconfinedTestDispatcher()

    private lateinit var fileManager: WallpaperFileManager

    @Before
    fun setup() {
        wallpapersFolder = File(tempFolder.root, "wallpapers")
        fileManager = WallpaperFileManager(
            storageRootDirectory = tempFolder.root,
            coroutineDispatcher = dispatcher,
        )
    }

    @Test
    fun `GIVEN wallpaper directory exists WHEN looked up THEN wallpaper created with correct name`() = runTest {
        val wallpaperName = "name"
        createAllFiles(wallpaperName)

        val result = fileManager.lookupExpiredWallpaper(wallpaperName)

        val expected = generateWallpaper(name = wallpaperName)
        assertEquals(expected, result)
    }

    @Test
    fun `GIVEN portrait file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        val wallpaperName = "name"
        File(wallpapersFolder, "$wallpaperName/landscape.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/thumbnail.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(wallpaperName)

        assertEquals(null, result)
    }

    @Test
    fun `GIVEN landscape file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        val wallpaperName = "name"
        File(wallpapersFolder, "$wallpaperName/portrait.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/thumbnail.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(wallpaperName)

        assertEquals(null, result)
    }

    @Test
    fun `GIVEN thumbnail file missing in directories WHEN expired wallpaper looked up THEN null returned`() = runTest {
        val wallpaperName = "name"
        File(wallpapersFolder, "$wallpaperName/portrait.png").apply {
            mkdirs()
            createNewFile()
        }
        File(wallpapersFolder, "$wallpaperName/landscape.png").apply {
            mkdirs()
            createNewFile()
        }

        val result = fileManager.lookupExpiredWallpaper(wallpaperName)

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
        textColor = null,
        cardColor = null,
        thumbnailFileState = Wallpaper.ImageFileState.Downloaded,
        collection = Wallpaper.DefaultCollection
    )
}
