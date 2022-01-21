package org.mozilla.fenix.wallpapers

import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings

class WallpaperManagerTest {

    private val fakeWallpaper = Wallpaper(
        name = "fake wallpaper",
        portraitPath = "",
        landscapePath = "",
        isDark = false,
        isThemed = false,
    )
    private val fakeThemedWallpaper = Wallpaper(
        name = "fake themed wallpaper",
        portraitPath = "",
        landscapePath = "",
        isDark = false,
        isThemed = true,
    )
    private val fakeWallpapers = listOf(fakeWallpaper, fakeThemedWallpaper)

    private val mockSettings: Settings = mockk()
    private val mockStorage: WallpaperStorage = mockk {
        every { loadAll() } returns fakeWallpapers
    }
    private val mockMetrics: MetricController = mockk()

    @Test
    fun `WHEN wallpaper set THEN current wallpaper updated in settings`() {
        every { mockMetrics.track(any()) } just runs
        val currentCaptureSlot = slot<String>()
        val updatedWallpaper = fakeWallpaper
        every { mockSettings.currentWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.currentWallpaper = capture(currentCaptureSlot) } just runs
        every { mockSettings.previousWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.previousWallpaper = capture(slot()) } just runs

        val wallpaperManager = WallpaperManager(mockSettings, mockStorage, mockMetrics)
        wallpaperManager.currentWallpaper = updatedWallpaper

        assertEquals(updatedWallpaper.name, currentCaptureSlot.captured)
    }

    @Test
    fun `WHEN wallpaper set THEN current wallpaper saved as previous in settings`() {
        every { mockMetrics.track(any()) } just runs
        val previousCaptureSlot = slot<String>()
        val currentWallpaper = fakeWallpaper
        every { mockSettings.currentWallpaper } returns currentWallpaper.name
        every { mockSettings.currentWallpaper = capture(slot()) } just runs
        every { mockSettings.previousWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.previousWallpaper = capture(previousCaptureSlot) } just runs

        val wallpaperManager = WallpaperManager(mockSettings, mockStorage, mockMetrics)
        wallpaperManager.currentWallpaper = WallpaperManager.defaultWallpaper

        assertEquals(currentWallpaper.name, previousCaptureSlot.captured)
    }

    @Test
    fun `WHEN wallpaper is initially fetched from settings THEN selection metric reported`() {
        val defaultWallpaper = WallpaperManager.defaultWallpaper
        every { mockSettings.previousWallpaper } returns defaultWallpaper.name
        every { mockSettings.currentWallpaper } returns defaultWallpaper.name
        every { mockMetrics.track(any()) } just runs

        WallpaperManager(mockSettings, mockStorage, mockMetrics)

        verify { mockMetrics.track(Event.Wallpaper.WallpaperSelected(defaultWallpaper)) }
    }

    @Test
    fun `GIVEN previous wallpaper was not default WHEN current wallpaper is loaded as default THEN selection metric reported and previous reset`() {
        val defaultWallpaper = WallpaperManager.defaultWallpaper
        val previousSlot = slot<String>()
        every { mockSettings.previousWallpaper } returns fakeWallpaper.name
        every { mockSettings.previousWallpaper = capture(previousSlot) } just runs
        every { mockSettings.currentWallpaper } returns defaultWallpaper.name
        every { mockMetrics.track(any()) } just runs

        WallpaperManager(mockSettings, mockStorage, mockMetrics)

        assertEquals(defaultWallpaper.name, previousSlot.captured)
        verify { mockMetrics.track(Event.Wallpaper.WallpaperResetToDefault) }
        verify { mockMetrics.track(Event.Wallpaper.WallpaperSelected(defaultWallpaper)) }
    }

    @Test
    fun `WHEN wallpaper updated THEN wallpaper applied metric recorded`() {
        val defaultWallpaper = WallpaperManager.defaultWallpaper
        every { mockSettings.previousWallpaper } returns defaultWallpaper.name
        every { mockSettings.previousWallpaper = capture(slot()) } just runs
        every { mockSettings.currentWallpaper } returns defaultWallpaper.name
        every { mockSettings.currentWallpaper = capture(slot()) } just runs
        every { mockMetrics.track(any()) } just runs

        val manager = WallpaperManager(mockSettings, mockStorage, mockMetrics)
        manager.updateWallpaper(mockk(relaxed = true), defaultWallpaper)

        verify { mockMetrics.track(Event.Wallpaper.NewWallpaperApplied(defaultWallpaper)) }
    }

    @Test
    fun `WHEN wallpaper discovered metric not previously recorded WHEN metric record attempted THEN metric sent`() {
        every { mockSettings.wallpapersDiscovered } returns false
        every { mockSettings.previousWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.currentWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.wallpapersDiscovered = true } just runs
        every { mockMetrics.track(any()) } just runs

        val wallpaperManager = WallpaperManager(mockSettings, mockStorage, mockMetrics)

        wallpaperManager.recordDiscoveredMetric()

        verify { mockMetrics.track(Event.Wallpaper.DiscoveredFeature) }
        verify { mockSettings.wallpapersDiscovered = true }
    }

    @Test
    fun `WHEN wallpaper discovered metric previously recorded WHEN metric record attempted THEN metric not sent`() {
        every { mockSettings.wallpapersDiscovered } returns true
        every { mockSettings.previousWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockSettings.currentWallpaper } returns WallpaperManager.defaultWallpaper.name
        every { mockMetrics.track(any()) } just runs

        val wallpaperManager = WallpaperManager(mockSettings, mockStorage, mockMetrics)

        wallpaperManager.recordDiscoveredMetric()

        verify(exactly = 0) { mockMetrics.track(Event.Wallpaper.DiscoveredFeature) }
    }
}
