/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.Bitmap
import android.widget.ImageView
import androidx.core.view.isVisible
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.cancel
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.WallpaperAction.UpdateCurrentWallpaper
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpapersUseCases

@RunWith(FenixRobolectricTestRunner::class)
class WallpapersObserverTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN the observer is created THEN start observing the store`() {
        val appStore: AppStore = mockk(relaxed = true) {
            every { observeManually(any()) } answers { mockk(relaxed = true) }
        }

        val observer = getObserver(appStore)

        assertNotNull(observer.observeWallpapersStoreSubscription)
    }

    @Test
    fun `GIVEN a certain wallpaper is chosen WHEN the state is updated with that wallpaper THEN load it it`() = runTestOnMain {
        val appStore = AppStore()
        val settings: Settings = mockk { every { currentWallpaperName } returns "Test" }
        val wallpaper: Wallpaper = mockk { every { name } returns "Test" }
        val observer = spyk(
            getObserver(appStore, settings, mockk(relaxed = true), mockk(relaxed = true)),
        )

        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()
        appStore.dispatch(UpdateCurrentWallpaper(wallpaper)).joinBlocking()
        appStore.waitUntilIdle()

        coVerify { observer.loadWallpaper(any()) }
        coVerify { observer.applyCurrentWallpaper() }
    }

    @Test
    fun `GIVEN a certain wallpaper is chosen WHEN the state updated with another wallpaper THEN avoid loading it`() = runTestOnMain {
        val appStore = AppStore()
        val settings: Settings = mockk { every { currentWallpaperName } returns "Test" }
        val otherWallpaper: Wallpaper = mockk { every { name } returns "Other" }
        val observer = spyk(
            getObserver(appStore, settings, mockk(relaxed = true), mockk(relaxed = true)),
        )

        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()
        appStore.dispatch(UpdateCurrentWallpaper(otherWallpaper)).joinBlocking()
        appStore.waitUntilIdle()

        coVerify(exactly = 0) { observer.loadWallpaper(any()) }
        coVerify(exactly = 0) { observer.applyCurrentWallpaper() }
    }

    @Test
    fun `GIVEN a wallpaper is SHOWN WHEN the wallpaper is updated to the current one THEN don't try showing the same wallpaper again`() {
        val appStore = AppStore()
        val settings: Settings = mockk { every { currentWallpaperName } returns "Test" }
        val wallpaper: Wallpaper = mockk { every { name } returns "Test" }
        val wallpapersUseCases: WallpapersUseCases = mockk { coEvery { loadBitmap(any()) } returns null }
        val observer = spyk(getObserver(appStore, settings, wallpapersUseCases, mockk(relaxed = true))) {
            coEvery { loadWallpaper(any()) } just Runs
            coEvery { applyCurrentWallpaper() } just Runs
        }

        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()
        appStore.dispatch(UpdateCurrentWallpaper(wallpaper)).joinBlocking()
        appStore.waitUntilIdle()
        coVerify(exactly = 1) { observer.loadWallpaper(any()) }
        coVerify(exactly = 1) { observer.applyCurrentWallpaper() }

        appStore.dispatch(UpdateCurrentWallpaper(wallpaper)).joinBlocking()
        appStore.waitUntilIdle()
        coVerify(exactly = 1) { observer.loadWallpaper(any()) }
        coVerify(exactly = 1) { observer.applyCurrentWallpaper() }
    }

    @Test
    fun `GIVEN the store was observed for updates WHEN the lifecycle owner is destroyed THEN stop observing the store`() {
        val observer = getObserver(mockk(relaxed = true))
        observer.observeWallpapersStoreSubscription = mockk(relaxed = true)
        observer.wallpapersScope = mockk {
            every { cancel() } just Runs
        }

        observer.onDestroy(mockk())

        verify { observer.wallpapersScope.cancel() }
        verify { observer.observeWallpapersStoreSubscription!!.unsubscribe() }
    }

    @Test
    fun `GIVEN a wallpaper image is available WHEN asked to apply the current wallpaper THEN show set it to the wallpaper ImageView`() = runTestOnMain {
        val imageView: ImageView = mockk(relaxed = true)
        val observer = getObserver(wallpaperImageView = imageView)
        val image: Bitmap = mockk()
        observer.currentWallpaperImage = image
        observer.isWallpaperLoaded.complete(Unit)

        observer.applyCurrentWallpaper()

        verify { imageView.setImageBitmap(image) }
        verify { imageView.isVisible = true }
    }

    fun `GIVEN no wallpaper image is available WHEN asked to apply the current wallpaper THEN hide the wallpaper ImageView`() = runTestOnMain {
        val imageView: ImageView = mockk()
        val observer = getObserver(wallpaperImageView = imageView)
        observer.isWallpaperLoaded.complete(Unit)

        observer.applyCurrentWallpaper()

        verify { imageView.isVisible = false }
        verify(exactly = 0) { imageView.setImageBitmap(any()) }
    }

    @Test
    fun `GIVEN the default wallpaper WHEN asked to load it THEN cache that the current image is null`() = runTestOnMain {
        val observer = getObserver()
        observer.currentWallpaperImage = mockk()

        observer.loadWallpaper(Wallpaper.Default)

        assertNull(observer.currentWallpaperImage)
    }

    @Test
    fun `GIVEN a custom wallpaper WHEN asked to load it THEN cache it's bitmap`() = runTestOnMain {
        val bitmap: Bitmap = mockk()
        val wallpaper: Wallpaper = mockk()
        val usecases: WallpapersUseCases = mockk {
            coEvery { loadBitmap(wallpaper) } returns bitmap
        }
        val observer = getObserver(wallpapersUseCases = usecases)

        observer.loadWallpaper(wallpaper)

        assertEquals(bitmap, observer.currentWallpaperImage)
    }

    private fun getObserver(
        appStore: AppStore = mockk(relaxed = true),
        settings: Settings = mockk(),
        wallpapersUseCases: WallpapersUseCases = mockk(),
        wallpaperImageView: ImageView = mockk(),
        backgroundWorkDispatcher: CoroutineDispatcher = coroutinesTestRule.testDispatcher,
    ) = WallpapersObserver(
        appStore = appStore,
        settings = settings,
        wallpapersUseCases = wallpapersUseCases,
        wallpaperImageView = wallpaperImageView,
        backgroundWorkDispatcher = backgroundWorkDispatcher,
    )
}
