package org.mozilla.fenix.wallpapers

import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings

class WallpaperManagerTest {

    private val mockSettings: Settings = mockk()
    private val mockStorage: WallpaperStorage = mockk {
        every { loadAll() } returns listOf()
    }
    private val mockMetrics: MetricController = mockk()

    @Test
    fun `WHEN wallpaper set THEN current wallpaper updated in settings`() {
        every { mockMetrics.track(any()) } just runs

        val currentCaptureSlot = slot<String>()
        every { mockSettings.currentWallpaper } returns "a different name"
        every { mockSettings.currentWallpaper = capture(currentCaptureSlot) } just runs

        val updatedWallpaper = WallpaperManager.defaultWallpaper
        val wallpaperManager = WallpaperManager(mockSettings, mockStorage)
        wallpaperManager.currentWallpaper = updatedWallpaper

        assertEquals(updatedWallpaper.name, currentCaptureSlot.captured)
    }
}
