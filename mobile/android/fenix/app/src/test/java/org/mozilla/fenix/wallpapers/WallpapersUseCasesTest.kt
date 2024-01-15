/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.wallpapers

import android.content.Context
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.toHexColor
import org.mozilla.fenix.wallpapers.LegacyWallpaperMigration.Companion.TURNING_RED_PANDA_WALLPAPER_CARD_COLOR_DARK
import org.mozilla.fenix.wallpapers.LegacyWallpaperMigration.Companion.TURNING_RED_PANDA_WALLPAPER_CARD_COLOR_LIGHT
import org.mozilla.fenix.wallpapers.LegacyWallpaperMigration.Companion.TURNING_RED_PANDA_WALLPAPER_NAME
import org.mozilla.fenix.wallpapers.LegacyWallpaperMigration.Companion.TURNING_RED_WALLPAPER_TEXT_COLOR
import java.io.File
import java.util.*
import kotlin.random.Random

class WallpapersUseCasesTest {

    // initialize this once, so it can be shared throughout tests
    private val baseFakeDate = Date()
    private val fakeCalendar = Calendar.getInstance()

    private val appStore = AppStore()
    private val mockSettings = mockk<Settings> {
        every { currentWallpaperTextColor } returns 0L
        every { currentWallpaperTextColor = any() } just Runs
        every { currentWallpaperCardColorLight } returns 0L
        every { currentWallpaperCardColorLight = any() } just Runs
        every { currentWallpaperCardColorDark } returns 0L
        every { currentWallpaperCardColorDark = any() } just Runs
        every { shouldMigrateLegacyWallpaper } returns false
        every { shouldMigrateLegacyWallpaper = any() } just Runs
        every { shouldMigrateLegacyWallpaperCardColors } returns false
        every { shouldMigrateLegacyWallpaperCardColors = any() } just Runs
    }
    private lateinit var mockMigrationHelper: LegacyWallpaperMigration

    private val mockMetadataFetcher = mockk<WallpaperMetadataFetcher>()
    private val mockDownloader = mockk<WallpaperDownloader> {
        coEvery { downloadWallpaper(any()) } returns mockk()
    }
    private val mockFileManager = mockk<WallpaperFileManager> {
        coEvery { clean(any(), any()) } returns mockk()
    }

    private val mockFolder: File = mockk()
    private val downloadWallpaper: (Wallpaper) -> Wallpaper.ImageFileState = mockk(relaxed = true)

    @Before
    fun setup() {
        mockMigrationHelper = spyk(
            LegacyWallpaperMigration(
                storageRootDirectory = mockFolder,
                settings = mockSettings,
                downloadWallpaper,
            ),
        )
    }

    @Test
    fun `WHEN initializing THEN the default wallpaper is not downloaded`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        appStore.waitUntilIdle()
        coVerify(exactly = 0) { mockDownloader.downloadWallpaper(Wallpaper.Default) }
    }

    @Test
    fun `WHEN initializing THEN default wallpaper is included in available wallpapers`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        appStore.waitUntilIdle()
        assertTrue(appStore.state.wallpaperState.availableWallpapers.contains(Wallpaper.Default))
    }

    @Test
    fun `GIVEN wallpapers that expired WHEN invoking initialize use case THEN expired wallpapers are filtered out and cleaned up`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val fakeExpiredRemoteWallpapers = listOf("expired").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.BEFORE, name)
        }
        val possibleWallpapers = fakeRemoteWallpapers + fakeExpiredRemoteWallpapers
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns possibleWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        val expectedFilteredWallpaper = fakeExpiredRemoteWallpapers[0]
        appStore.waitUntilIdle()
        assertFalse(appStore.state.wallpaperState.availableWallpapers.contains(expectedFilteredWallpaper))
        coVerify { mockFileManager.clean(Wallpaper.Default, fakeRemoteWallpapers) }
    }

    @Test
    fun `GIVEN wallpapers that expired and an expired one is selected WHEN invoking initialize use case THEN selected wallpaper is not filtered out`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val expiredWallpaper = makeFakeRemoteWallpaper(TimeRelation.BEFORE, "expired")
        val allWallpapers = listOf(expiredWallpaper) + fakeRemoteWallpapers
        every { mockSettings.currentWallpaperName } returns "expired"
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns expiredWallpaper
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns allWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        val expectedWallpaper = expiredWallpaper.copy(
            thumbnailFileState = Wallpaper.ImageFileState.Downloaded,
        )
        appStore.waitUntilIdle()
        assertTrue(appStore.state.wallpaperState.availableWallpapers.contains(expectedWallpaper))
        assertEquals(expiredWallpaper, appStore.state.wallpaperState.currentWallpaper)
    }

    @Test
    fun `GIVEN wallpapers that expired and an expired one is selected and card colors have not been migrated WHEN invoking initialize use case THEN migrate card colors`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val expiredWallpaper = makeFakeRemoteWallpaper(TimeRelation.BEFORE, TURNING_RED_PANDA_WALLPAPER_NAME)
        val allWallpapers = listOf(expiredWallpaper) + fakeRemoteWallpapers
        every { mockSettings.currentWallpaperName } returns TURNING_RED_PANDA_WALLPAPER_NAME
        every { mockSettings.shouldMigrateLegacyWallpaperCardColors } returns true
        every { mockSettings.currentWallpaperTextColor } returns TURNING_RED_WALLPAPER_TEXT_COLOR.toHexColor()
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns expiredWallpaper
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns allWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        appStore.waitUntilIdle()

        verify { mockMigrationHelper.migrateExpiredWallpaperCardColors() }
        verify { mockSettings.currentWallpaperCardColorLight = TURNING_RED_PANDA_WALLPAPER_CARD_COLOR_LIGHT.toHexColor() }
        verify { mockSettings.currentWallpaperCardColorDark = TURNING_RED_PANDA_WALLPAPER_CARD_COLOR_DARK.toHexColor() }
    }

    @Test
    fun `GIVEN wallpapers that are in promotions outside of locale WHEN invoking initialize use case THEN promotional wallpapers are filtered out`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val locale = "en-CA"
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            locale,
        ).invoke()

        appStore.waitUntilIdle()
        assertEquals(1, appStore.state.wallpaperState.availableWallpapers.size)
        assertEquals(Wallpaper.Default, appStore.state.wallpaperState.availableWallpapers[0])
    }

    @Test
    fun `GIVEN available wallpapers WHEN invoking initialize use case THEN available wallpaper thumbnails downloaded`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        for (fakeRemoteWallpaper in fakeRemoteWallpapers) {
            coVerify { mockDownloader.downloadThumbnail(fakeRemoteWallpaper) }
        }
    }

    @Test
    fun `GIVEN available wallpapers WHEN invoking initialize use case THEN thumbnails downloaded and the app store state is updated to reflect that`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        for (fakeRemoteWallpaper in fakeRemoteWallpapers) {
            coVerify { mockDownloader.downloadThumbnail(fakeRemoteWallpaper) }
        }
        appStore.waitUntilIdle()
        assertTrue(
            appStore.state.wallpaperState.availableWallpapers.all {
                it.thumbnailFileState == Wallpaper.ImageFileState.Downloaded
            },
        )
    }

    @Test
    fun `GIVEN thumbnail download fails WHEN invoking initialize use case THEN the app store state is updated to reflect that`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val failedWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "failed")
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns listOf(failedWallpaper) + fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded
        coEvery { mockDownloader.downloadThumbnail(failedWallpaper) } returns Wallpaper.ImageFileState.Error

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        val expectedWallpaper = failedWallpaper.copy(thumbnailFileState = Wallpaper.ImageFileState.Error)
        appStore.waitUntilIdle()
        assertTrue(appStore.state.wallpaperState.availableWallpapers.contains(expectedWallpaper))
    }

    @Test
    fun `GIVEN a wallpaper has not been selected WHEN invoking initialize use case THEN app store contains default`() = runTest {
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        every { mockSettings.currentWallpaperName } returns ""
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns fakeRemoteWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        appStore.waitUntilIdle()
        assertTrue(appStore.state.wallpaperState.currentWallpaper == Wallpaper.Default)
    }

    @Test
    fun `GIVEN a wallpaper is selected and there are available wallpapers WHEN invoking initialize use case THEN these are dispatched to the app store`() = runTest {
        val selectedWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "selected")
        val fakeRemoteWallpapers = listOf("first", "second", "third").map { name ->
            makeFakeRemoteWallpaper(TimeRelation.LATER, name)
        }
        val possibleWallpapers = listOf(selectedWallpaper) + fakeRemoteWallpapers
        every { mockSettings.currentWallpaperName } returns selectedWallpaper.name
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        coEvery { mockMetadataFetcher.downloadWallpaperList() } returns possibleWallpapers
        coEvery { mockDownloader.downloadThumbnail(any()) } returns Wallpaper.ImageFileState.Downloaded

        WallpapersUseCases.DefaultInitializeWallpaperUseCase(
            appStore,
            mockDownloader,
            mockFileManager,
            mockMetadataFetcher,
            mockMigrationHelper,
            mockSettings,
            "en-US",
        ).invoke()

        val expectedWallpapers = (listOf(Wallpaper.Default) + possibleWallpapers).map {
            it.copy(thumbnailFileState = Wallpaper.ImageFileState.Downloaded)
        }
        appStore.waitUntilIdle()
        assertEquals(selectedWallpaper, appStore.state.wallpaperState.currentWallpaper)
        assertEquals(expectedWallpapers, appStore.state.wallpaperState.availableWallpapers)
    }

    @Test
    fun `GIVEN wallpaper downloaded WHEN selecting a wallpaper THEN storage updated and app store receives dispatch`() = runTest {
        val selectedWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "selected")
        val slot = slot<String>()
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        every { mockSettings.currentWallpaperName } returns ""
        every { mockSettings.currentWallpaperName = capture(slot) } just runs
        coEvery { mockFileManager.wallpaperImagesExist(selectedWallpaper) } returns true

        val wallpaperFileState = WallpapersUseCases.DefaultSelectWallpaperUseCase(
            mockSettings,
            appStore,
            mockFileManager,
            mockDownloader,
        ).invoke(selectedWallpaper)

        appStore.waitUntilIdle()
        assertEquals(selectedWallpaper.name, slot.captured)
        assertEquals(selectedWallpaper, appStore.state.wallpaperState.currentWallpaper)
        assertEquals(wallpaperFileState, Wallpaper.ImageFileState.Downloaded)
    }

    @Test
    fun `GIVEN wallpaper is not downloaded WHEN selecting a wallpaper and download succeeds THEN storage updated and app store receives dispatch`() = runTest {
        val selectedWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "selected")
        val slot = slot<String>()
        val mockAppStore = mockk<AppStore>(relaxed = true)
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        every { mockSettings.currentWallpaperName } returns ""
        every { mockSettings.currentWallpaperName = capture(slot) } just runs
        coEvery { mockFileManager.wallpaperImagesExist(selectedWallpaper) } returns false
        coEvery { mockDownloader.downloadWallpaper(selectedWallpaper) } returns Wallpaper.ImageFileState.Downloaded

        val wallpaperFileState = WallpapersUseCases.DefaultSelectWallpaperUseCase(
            mockSettings,
            mockAppStore,
            mockFileManager,
            mockDownloader,
        ).invoke(selectedWallpaper)

        verify { mockAppStore.dispatch(AppAction.WallpaperAction.UpdateWallpaperDownloadState(selectedWallpaper, Wallpaper.ImageFileState.Downloading)) }
        verify { mockAppStore.dispatch(AppAction.WallpaperAction.UpdateWallpaperDownloadState(selectedWallpaper, Wallpaper.ImageFileState.Downloaded)) }
        verify { mockAppStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(selectedWallpaper)) }
        assertEquals(wallpaperFileState, Wallpaper.ImageFileState.Downloaded)
    }

    @Test
    fun `GIVEN wallpaper is not downloaded WHEN selecting a wallpaper and any download fails THEN wallpaper not set and app store receives dispatch`() = runTest {
        val selectedWallpaper = makeFakeRemoteWallpaper(TimeRelation.LATER, "selected")
        val slot = slot<String>()
        val mockAppStore = mockk<AppStore>(relaxed = true)
        coEvery { mockFileManager.lookupExpiredWallpaper(any()) } returns null
        every { mockSettings.currentWallpaperName } returns ""
        every { mockSettings.currentWallpaperName = capture(slot) } just runs
        coEvery { mockFileManager.wallpaperImagesExist(selectedWallpaper) } returns false
        coEvery { mockDownloader.downloadWallpaper(selectedWallpaper) } returns Wallpaper.ImageFileState.Error

        val wallpaperFileState = WallpapersUseCases.DefaultSelectWallpaperUseCase(
            mockSettings,
            mockAppStore,
            mockFileManager,
            mockDownloader,
        ).invoke(selectedWallpaper)

        verify { mockAppStore.dispatch(AppAction.WallpaperAction.UpdateWallpaperDownloadState(selectedWallpaper, Wallpaper.ImageFileState.Downloading)) }
        verify { mockAppStore.dispatch(AppAction.WallpaperAction.UpdateWallpaperDownloadState(selectedWallpaper, Wallpaper.ImageFileState.Error)) }
        assertEquals(wallpaperFileState, Wallpaper.ImageFileState.Error)
    }

    @Test
    fun `GIVEN a wallpaper with no text color WHEN it is is selected THEN persist the wallpaper name and missing text color and dispatch the update`() {
        every { mockSettings.currentWallpaperName = any() } just Runs
        val appStore = mockk<AppStore>(relaxed = true)
        val wallpaperFileState = WallpapersUseCases.DefaultSelectWallpaperUseCase(
            settings = mockSettings,
            appStore = appStore,
            fileManager = mockk(),
            downloader = mockk(),
        )
        val wallpaper: Wallpaper = mockk {
            every { name } returns "Test"
            every { textColor } returns null
            every { cardColorLight } returns null
            every { cardColorDark } returns null
        }

        wallpaperFileState.selectWallpaper(wallpaper)

        verify { mockSettings.currentWallpaperName = "Test" }
        verify { mockSettings.currentWallpaperTextColor = 0L }
        verify { appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(wallpaper)) }
    }

    @Test
    fun `GIVEN a wallpaper with available text color WHEN it is is selected THEN persist the wallpaper name and text color and dispatch the update`() {
        every { mockSettings.currentWallpaperName = any() } just Runs
        val appStore = mockk<AppStore>(relaxed = true)
        val wallpaperFileState = WallpapersUseCases.DefaultSelectWallpaperUseCase(
            settings = mockSettings,
            appStore = appStore,
            fileManager = mockk(),
            downloader = mockk(),
        )
        val wallpaper: Wallpaper = mockk {
            every { name } returns "Test"
            every { textColor } returns 321L
            every { cardColorLight } returns 321L
            every { cardColorDark } returns 321L
        }

        wallpaperFileState.selectWallpaper(wallpaper)

        verify { mockSettings.currentWallpaperName = "Test" }
        verify { mockSettings.currentWallpaperTextColor = 321L }
        verify { appStore.dispatch(AppAction.WallpaperAction.UpdateCurrentWallpaper(wallpaper)) }
    }

    @Test
    fun `GIVEN the context WHEN bitmap is loaded THEN loadWallpaperFromDisk method is called with the correct context and wallpaper`() =
        runTest {
            val wallpaper: Wallpaper = mockk {
                every { name } returns "test"
            }
            val context = mockk<Context>(relaxed = true)
            val defaultLoadBitmapUseCase = spyk(WallpapersUseCases.DefaultLoadBitmapUseCase { mockFolder })
            coEvery { defaultLoadBitmapUseCase.loadWallpaperFromDisk(context, wallpaper) } returns mockk()

            defaultLoadBitmapUseCase.invoke(context, wallpaper)

            coVerify { defaultLoadBitmapUseCase.loadWallpaperFromDisk(context, wallpaper) }
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
        isInPromo: Boolean = true,
    ): Wallpaper {
        fakeCalendar.time = baseFakeDate
        when (timeRelation) {
            TimeRelation.BEFORE -> fakeCalendar.add(Calendar.DATE, -5)
            TimeRelation.NOW -> Unit
            TimeRelation.LATER -> fakeCalendar.add(Calendar.DATE, 5)
        }
        val relativeTime = fakeCalendar.time
        return if (isInPromo) {
            Wallpaper(
                name = name,
                collection = Wallpaper.Collection(
                    name = Wallpaper.firefoxCollectionName,
                    heading = null,
                    description = null,
                    availableLocales = listOf("en-US"),
                    startDate = null,
                    endDate = relativeTime,
                    learnMoreUrl = null,
                ),
                textColor = Random.nextLong(),
                cardColorLight = Random.nextLong(),
                cardColorDark = Random.nextLong(),
                thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                assetsFileState = Wallpaper.ImageFileState.Unavailable,
            )
        } else {
            Wallpaper(
                name = name,
                collection = Wallpaper.Collection(
                    name = Wallpaper.firefoxCollectionName,
                    heading = null,
                    description = null,
                    availableLocales = null,
                    startDate = null,
                    endDate = relativeTime,
                    learnMoreUrl = null,
                ),
                textColor = Random.nextLong(),
                cardColorLight = Random.nextLong(),
                cardColorDark = Random.nextLong(),
                thumbnailFileState = Wallpaper.ImageFileState.Unavailable,
                assetsFileState = Wallpaper.ImageFileState.Unavailable,
            )
        }
    }
}
