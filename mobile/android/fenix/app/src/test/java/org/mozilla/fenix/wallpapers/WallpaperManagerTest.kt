package org.mozilla.fenix.wallpapers

import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.utils.Settings

class WallpaperManagerTest {

    private val mockSettings: Settings = mockk()

    @Test
    fun `GIVEN no custom wallpaper set WHEN checking whether the current wallpaper should be default THEN return true`() {
        every { mockSettings.currentWallpaper } returns ""

        val result = WallpaperManager.isDefaultTheCurrentWallpaper(mockSettings)

        assertTrue(result)
    }

    @Test
    fun `GIVEN the default wallpaper is set to be shown WHEN checking whether the current wallpaper should be default THEN return true`() {
        every { mockSettings.currentWallpaper } returns WallpaperManager.defaultWallpaper.name

        val result = WallpaperManager.isDefaultTheCurrentWallpaper(mockSettings)

        assertTrue(result)
    }

    @Test
    fun `GIVEN a custom wallpaper is set to be shown WHEN checking whether the current wallpaper should be default THEN return false`() {
        every { mockSettings.currentWallpaper } returns "test"

        val result = WallpaperManager.isDefaultTheCurrentWallpaper(mockSettings)

        assertFalse(result)
    }
}
