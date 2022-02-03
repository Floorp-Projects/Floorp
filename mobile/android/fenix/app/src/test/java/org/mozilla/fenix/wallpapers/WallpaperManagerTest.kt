package org.mozilla.fenix.wallpapers

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import kotlinx.coroutines.test.runBlockingTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings
import java.util.Calendar
import java.util.Date

class WallpaperManagerTest {

    // initialize this once, so it can be shared throughout tests
    private val baseFakeDate = Date()
    private val fakeCalendar = Calendar.getInstance()

    private val mockSettings: Settings = mockk()
    private val mockMetrics: MetricController = mockk()
    private val mockDownloader: WallpaperDownloader = mockk {
        coEvery { downloadWallpaper(any()) } just runs
    }
    private val mockFileManager: WallpaperFileManager = mockk {
        every { clean(any(), any()) } just runs
    }

    @Test
    fun `WHEN wallpaper set THEN current wallpaper updated in settings`() {
        every { mockMetrics.track(any()) } just runs

        val currentCaptureSlot = slot<String>()
        every { mockSettings.currentWallpaper } returns ""
        every { mockSettings.currentWallpaper = capture(currentCaptureSlot) } just runs

        val updatedName = "new name"
        val updatedWallpaper = Wallpaper.Local.Firefox(updatedName, drawableId = 0)
        val wallpaperManager = WallpaperManager(mockSettings, mockk(), mockFileManager, mockk(), listOf())
        wallpaperManager.currentWallpaper = updatedWallpaper

        assertEquals(updatedWallpaper.name, currentCaptureSlot.captured)
    }

    @Test
    fun `GIVEN no remote wallpapers expired WHEN downloading remote wallpapers THEN all downloaded`() = runBlockingTest {
        every { mockSettings.currentWallpaper } returns ""
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            mockk(),
            allWallpapers = fakeRemoteWallpapers
        )
        wallpaperManager.downloadAllRemoteWallpapers()

        for (fakeRemoteWallpaper in fakeRemoteWallpapers) {
            coVerify { mockDownloader.downloadWallpaper(fakeRemoteWallpaper) }
        }
    }

    @Test
    fun `GIVEN some expired wallpapers WHEN initialized THEN wallpapers are not available`() {
        every { mockSettings.currentWallpaper } returns ""
        val expiredRemoteWallpaper = makeFakeRemoteWallpaper(TimeRelation.BEFORE, "expired")
        val activeRemoteWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "expired")
        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            mockk(),
            allWallpapers = listOf(expiredRemoteWallpaper, activeRemoteWallpaper)
        )

        assertFalse(wallpaperManager.wallpapers.contains(expiredRemoteWallpaper))
        assertTrue(wallpaperManager.wallpapers.contains(activeRemoteWallpaper))
    }

    @Test
    fun `GIVEN current wallpaper is expired THEN it is available as expired even when others are filtered`() {
        val currentWallpaperName = "named"
        val currentExpiredWallpaper = makeFakeRemoteWallpaper(TimeRelation.BEFORE, name = currentWallpaperName)
        every { mockSettings.currentWallpaper } returns currentWallpaperName
        val expiredRemoteWallpaper = makeFakeRemoteWallpaper(TimeRelation.BEFORE, "expired")
        val expected = Wallpaper.Remote.Expired(currentWallpaperName)
        every { mockFileManager.lookupExpiredWallpaper(currentWallpaperName) } returns expected

        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            mockk(),
            allWallpapers = listOf(expiredRemoteWallpaper)
        )

        assertFalse(wallpaperManager.wallpapers.contains(expiredRemoteWallpaper))
        assertFalse(wallpaperManager.wallpapers.contains(currentExpiredWallpaper))
        assertEquals(expected, wallpaperManager.currentWallpaper)
    }

    @Test
    fun `GIVEN current wallpaper is expired THEN it is available even if not listed in initial parameter`() {
        val currentWallpaperName = "named"
        every { mockSettings.currentWallpaper } returns currentWallpaperName
        val expected = Wallpaper.Remote.Expired(currentWallpaperName)
        every { mockFileManager.lookupExpiredWallpaper(currentWallpaperName) } returns expected

        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            mockk(),
            allWallpapers = listOf()
        )

        assertEquals(expected, wallpaperManager.currentWallpaper)
    }

    private enum class TimeRelation {
        BEFORE,
        NOW,
        LATER,
    }

    /**
     * [timeRelation] should specify a time relative to the time the tests are run
     */
    private fun makeFakeRemoteWallpaper(
        timeRelation: TimeRelation,
        name: String = "name"
    ): Wallpaper.Remote {
        fakeCalendar.time = baseFakeDate
        when (timeRelation) {
            TimeRelation.BEFORE -> fakeCalendar.add(Calendar.DATE, -5)
            TimeRelation.NOW -> Unit
            TimeRelation.LATER -> fakeCalendar.add(Calendar.DATE, 5)
        }
        val relativeTime = fakeCalendar.time
        return Wallpaper.Remote.Focus(name = name, expirationDate = relativeTime)
    }
}
