package org.mozilla.fenix.wallpapers

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.utils.Settings
import java.util.Calendar
import java.util.Date

class WallpaperManagerTest {

    // initialize this once, so it can be shared throughout tests
    private val baseFakeDate = Date()
    private val fakeCalendar = Calendar.getInstance()

    private val mockSettings: Settings = mockk()
    private val mockDownloader: WallpaperDownloader = mockk {
        coEvery { downloadWallpaper(any()) } just runs
    }
    private val mockFileManager: WallpaperFileManager = mockk {
        every { clean(any(), any()) } just runs
    }

    @Test
    fun `WHEN wallpaper set THEN current wallpaper updated in settings`() {
        val currentCaptureSlot = slot<String>()
        every { mockSettings.currentWallpaper } returns ""
        every { mockSettings.currentWallpaper = capture(currentCaptureSlot) } just runs

        val updatedName = "new name"
        val updatedWallpaper = Wallpaper.Local.Firefox(updatedName, drawableId = 0)
        val wallpaperManager = WallpaperManager(mockSettings, mockk(), mockFileManager, "en-US", listOf())
        wallpaperManager.currentWallpaper = updatedWallpaper

        assertEquals(updatedWallpaper.name, currentCaptureSlot.captured)
    }

    @Test
    fun `GIVEN no remote wallpapers expired and locale in promo WHEN downloading remote wallpapers THEN all downloaded`() = runTest {
        every { mockSettings.currentWallpaper } returns ""
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            "en-US",
            allWallpapers = fakeRemoteWallpapers
        )
        wallpaperManager.downloadAllRemoteWallpapers()

        for (fakeRemoteWallpaper in fakeRemoteWallpapers) {
            coVerify { mockDownloader.downloadWallpaper(fakeRemoteWallpaper) }
        }
    }

    @Test
    fun `GIVEN no remote wallpapers expired and locale not in promo WHEN downloading remote wallpapers THEN none downloaded`() = runTest {
        every { mockSettings.currentWallpaper } returns ""
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            "en-CA",
            allWallpapers = fakeRemoteWallpapers
        )
        wallpaperManager.downloadAllRemoteWallpapers()

        for (fakeRemoteWallpaper in fakeRemoteWallpapers) {
            coVerify(exactly = 0) { mockDownloader.downloadWallpaper(fakeRemoteWallpaper) }
        }
    }

    @Test
    fun `GIVEN no remote wallpapers expired and locale not in promo WHEN downloading remote wallpapers THEN non-promo wallpapers downloaded`() = runTest {
        every { mockSettings.currentWallpaper } returns ""
        val fakePromoWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val fakeNonPromoWallpapers = listOf(makeFakeRemoteWallpaper(TimeRelation.LATER, "fourth", false))
        val fakeRemoteWallpapers = fakePromoWallpapers + fakeNonPromoWallpapers
        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            "en-CA",
            allWallpapers = fakeRemoteWallpapers
        )
        wallpaperManager.downloadAllRemoteWallpapers()

        for (wallpaper in fakePromoWallpapers) {
            coVerify(exactly = 0) { mockDownloader.downloadWallpaper(wallpaper) }
        }
        for (wallpaper in fakeNonPromoWallpapers) {
            coVerify { mockDownloader.downloadWallpaper(wallpaper) }
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
            "en-US",
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
        val expected = Wallpaper.Expired(currentWallpaperName)
        every { mockFileManager.lookupExpiredWallpaper(currentWallpaperName) } returns expected

        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            "en-US",
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
        val expected = Wallpaper.Expired(currentWallpaperName)
        every { mockFileManager.lookupExpiredWallpaper(currentWallpaperName) } returns expected

        val wallpaperManager = WallpaperManager(
            mockSettings,
            mockDownloader,
            mockFileManager,
            "en-US",
            allWallpapers = listOf()
        )

        assertEquals(expected, wallpaperManager.currentWallpaper)
    }

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
        name: String = "name",
        isInPromo: Boolean = true
    ): Wallpaper.Remote {
        fakeCalendar.time = baseFakeDate
        when (timeRelation) {
            TimeRelation.BEFORE -> fakeCalendar.add(Calendar.DATE, -5)
            TimeRelation.NOW -> Unit
            TimeRelation.LATER -> fakeCalendar.add(Calendar.DATE, 5)
        }
        val relativeTime = fakeCalendar.time
        return if (isInPromo) {
            Wallpaper.Remote.House(name = name, expirationDate = relativeTime)
        } else {
            Wallpaper.Remote.Firefox(name = name)
        }
    }
}
