package org.mozilla.fenix.wallpapers

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class LegacyWallpaperMigrationTest {
    @Rule
    @JvmField
    val tempFolder = TemporaryFolder()
    private lateinit var wallpapersFolder: File
    private lateinit var migrationHelper: LegacyWallpaperMigration
    private lateinit var portraitLightFolder: File
    private lateinit var portraitDarkFolder: File
    private lateinit var landscapeLightFolder: File
    private lateinit var landscapeDarkFolder: File

    @Before
    fun setup() {
        wallpapersFolder = File(tempFolder.root, "wallpapers")
        migrationHelper = LegacyWallpaperMigration(
            storageRootDirectory = tempFolder.root,
        )
    }

    @Test
    fun `WHEN the legacy wallpaper is migrated THEN the legacy wallpapers are deleted`() = runTest {
        val wallpaperName = "wallpaper1"

        createAllLegacyFiles(wallpaperName)

        migrationHelper.migrateLegacyWallpaper(wallpaperName)

        assertTrue(getAllFiles(wallpaperName).all { it.exists() })
        assertFalse(File(portraitLightFolder, "$wallpaperName.png").exists())
        assertFalse(File(portraitDarkFolder, "$wallpaperName.png").exists())
        assertFalse(File(landscapeLightFolder, "$wallpaperName.png").exists())
        assertFalse(File(landscapeDarkFolder, "$wallpaperName.png").exists())
    }

    @Test
    fun `GIVEN landscape legacy wallpaper is missing WHEN the wallpapers are migrated THEN the wallpaper is not migrated`() =
        runTest {
            val portraitOnlyWallpaperName = "portraitOnly"
            val completeWallpaperName = "legacy"
            createAllLegacyFiles(completeWallpaperName)
            File(landscapeLightFolder, "$portraitOnlyWallpaperName.png").apply {
                createNewFile()
            }
            File(landscapeDarkFolder, "$portraitOnlyWallpaperName.png").apply {
                createNewFile()
            }

            migrationHelper.migrateLegacyWallpaper(portraitOnlyWallpaperName)
            migrationHelper.migrateLegacyWallpaper(completeWallpaperName)

            assertTrue(getAllFiles(completeWallpaperName).all { it.exists() })
            assertFalse(getAllFiles(portraitOnlyWallpaperName).any { it.exists() })
        }

    @Test
    fun `GIVEN portrait legacy wallpaper is missing WHEN the wallpapers are migrated THEN the wallpaper is not migrated`() =
        runTest {
            val landscapeOnlyWallpaperName = "portraitOnly"
            val completeWallpaperName = "legacy"
            createAllLegacyFiles(completeWallpaperName)
            File(portraitLightFolder, "$landscapeOnlyWallpaperName.png").apply {
                createNewFile()
            }
            File(portraitDarkFolder, "$landscapeOnlyWallpaperName.png").apply {
                createNewFile()
            }

            migrationHelper.migrateLegacyWallpaper(landscapeOnlyWallpaperName)
            migrationHelper.migrateLegacyWallpaper(completeWallpaperName)

            assertTrue(getAllFiles(completeWallpaperName).all { it.exists() })
            assertFalse(getAllFiles(landscapeOnlyWallpaperName).any { it.exists() })
        }

    private fun createAllLegacyFiles(name: String) {
        if (!this::portraitLightFolder.isInitialized) {
            portraitLightFolder = tempFolder.newFolder("wallpapers", "portrait", "light")
            portraitDarkFolder = tempFolder.newFolder("wallpapers", "portrait", "dark")
            landscapeLightFolder = tempFolder.newFolder("wallpapers", "landscape", "light")
            landscapeDarkFolder = tempFolder.newFolder("wallpapers", "landscape", "dark")
        }

        File(portraitLightFolder, "$name.png").apply {
            createNewFile()
        }
        File(landscapeLightFolder, "$name.png").apply {
            createNewFile()
        }
        File(portraitDarkFolder, "$name.png").apply {
            createNewFile()
        }
        File(landscapeDarkFolder, "$name.png").apply {
            createNewFile()
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
}
