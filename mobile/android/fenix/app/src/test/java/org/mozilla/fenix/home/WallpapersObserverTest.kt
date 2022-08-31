/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.Bitmap
import android.widget.ImageView
import androidx.core.view.isVisible
import io.mockk.Called
import io.mockk.Runs
import io.mockk.coEvery
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.cancel
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.WallpaperAction.UpdateCurrentWallpaper
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.wallpapers.Wallpaper
import org.mozilla.fenix.wallpapers.WallpapersUseCases

@RunWith(FenixRobolectricTestRunner::class)
class WallpapersObserverTest {
    @Test
    fun `WHEN the observer is created THEN start observing the store`() {
        val appStore: AppStore = mockk(relaxed = true) {
            every { observeManually(any()) } answers { mockk(relaxed = true) }
        }

        val observer = WallpapersObserver(appStore, mockk(), mockk())

        assertNotNull(observer.observeWallpapersStoreSubscription)
    }

    @Test
    fun `WHEN asked to apply the wallpaper THEN show it`() {
        val appStore = AppStore()
        val observer = spyk(WallpapersObserver(appStore, mockk(), mockk())) {
            every { showWallpaper(any()) } just Runs
        }

        observer.applyCurrentWallpaper()

        verify { observer.showWallpaper(any()) }
    }

    @Test
    fun `GIVEN the store was observed for updates WHEN the lifecycle owner is destroyed THEN stop observing the store`() {
        val observer = WallpapersObserver(mockk(relaxed = true), mockk(), mockk())
        observer.observeWallpapersStoreSubscription = mockk(relaxed = true)
        observer.wallpapersScope = mockk {
            every { cancel() } just Runs
        }

        observer.onDestroy(mockk())

        verify { observer.wallpapersScope.cancel() }
        verify { observer.observeWallpapersStoreSubscription!!.unsubscribe() }
    }

    @Test
    fun `WHEN the wallpaper is updated THEN show the wallpaper`() {
        val appStore = AppStore()
        val observer = spyk(WallpapersObserver(appStore, mockk(relaxed = true), mockk(relaxed = true))) {
            every { showWallpaper(any()) } just Runs
        }

        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()

        val newWallpaper: Wallpaper = mockk(relaxed = true)
        appStore.dispatch(UpdateCurrentWallpaper(newWallpaper))
        appStore.waitUntilIdle()
        verify { observer.showWallpaper(newWallpaper) }
    }

    @Test
    fun `WHEN the wallpaper is updated to a new one THEN show the wallpaper`() {
        val appStore = AppStore()
        val wallpapersUseCases: WallpapersUseCases = mockk {
            coEvery { loadBitmap(any()) } returns null
        }
        val observer = spyk(WallpapersObserver(appStore, wallpapersUseCases, mockk(relaxed = true))) {
            every { showWallpaper(any()) } just Runs
        }

        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()
        verify { observer.showWallpaper(Wallpaper.Default) }

        val wallpaper: Wallpaper = mockk(relaxed = true)
        appStore.dispatch(UpdateCurrentWallpaper(wallpaper))
        appStore.waitUntilIdle()
        verify { observer.showWallpaper(wallpaper) }
    }

    @Test
    fun `WHEN the wallpaper is updated to the current one THEN don't try showing the same wallpaper again`() {
        val appStore = AppStore()
        val wallpapersUseCases: WallpapersUseCases = mockk {
            coEvery { loadBitmap(any()) } returns null
        }
        val observer = spyk(WallpapersObserver(appStore, wallpapersUseCases, mockk(relaxed = true))) {
            every { showWallpaper(any()) } just Runs
        }
        // Ignore the call on the real instance and call again "observeWallpaperUpdates"
        // on the spy to be able to verify the "showWallpaper" call in the spy.
        observer.observeWallpaperUpdates()

        val wallpaper: Wallpaper = mockk(relaxed = true)
        appStore.dispatch(UpdateCurrentWallpaper(wallpaper))
        appStore.waitUntilIdle()
        verify { observer.showWallpaper(wallpaper) }

        appStore.dispatch(UpdateCurrentWallpaper(wallpaper))
        appStore.waitUntilIdle()
        verify(exactly = 1) { observer.showWallpaper(wallpaper) }
    }

    @Test
    fun `GIVEN no wallpaper is provided WHEN asked to show the wallpaper THEN show the current one`() {
        val wallpaper: Wallpaper = mockk()
        val appStore: AppStore = mockk(relaxed = true) {
            every { state.wallpaperState.currentWallpaper } returns wallpaper
        }
        val observer = spyk(WallpapersObserver(appStore, mockk(relaxed = true), mockk(relaxed = true)))

        observer.showWallpaper()

        verify { observer.showWallpaper(wallpaper) }
    }

    fun `GiVEN the current wallpaper is the default one WHEN showing it THEN hide the wallpaper view`() {
        val wallpapersUseCases: WallpapersUseCases = mockk()
        val wallpaperView: ImageView = mockk(relaxed = true)
        val observer = WallpapersObserver(mockk(relaxed = true), wallpapersUseCases, wallpaperView)

        observer.showWallpaper(Wallpaper.Default)

        verify { wallpaperView.isVisible = false }
        verify { wallpapersUseCases wasNot Called }
    }

    @Test
    fun `GiVEN the current wallpaper is different than the default one WHEN showing it THEN load it's bitmap in the visible wallpaper view`() {
        val wallpaper: Wallpaper = mockk()
        val bitmap: Bitmap = mockk()
        val wallpapersUseCases: WallpapersUseCases = mockk {
            coEvery { loadBitmap(any()) } returns bitmap
        }
        val wallpaperView: ImageView = mockk(relaxed = true)
        val observer = WallpapersObserver(mockk(relaxed = true), wallpapersUseCases, wallpaperView)

        observer.showWallpaper(wallpaper)

        verify { wallpaperView.isVisible = true }
        verify { wallpaperView.setImageBitmap(bitmap) }
    }

    @Test
    fun `GIVEN the observer THEN use the main thread for showing the wallpaper`() {
        val wallpapersUseCases: WallpapersUseCases = mockk()
        val wallpaperView: ImageView = mockk(relaxed = true)

        val observer = WallpapersObserver(mockk(relaxed = true), wallpapersUseCases, wallpaperView)

        // Check that the context that would be used is Dispatchers.Main.immediate
        // Unfortunately I could not also test that this is actually used when "showWallpaper" is called.
        assertTrue(observer.wallpapersScope.toString().contains("Dispatchers.Main.immediate"))
    }
}
