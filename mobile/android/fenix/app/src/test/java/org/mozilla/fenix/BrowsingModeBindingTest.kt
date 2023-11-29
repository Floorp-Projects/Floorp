/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import android.view.Window
import android.view.WindowManager
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings

class BrowsingModeBindingTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var themeManager: ThemeManager
    private lateinit var window: Window
    private lateinit var settings: Settings
    private val retrieveWindow = { window }

    private lateinit var appStore: AppStore

    private lateinit var binding: BrowsingModeBinding

    @Before
    fun setup() {
        themeManager = mock()
        window = mock()
        settings = mock()
        whenever(window.clearFlags(anyInt())).then { }

        appStore = AppStore()
        // wait for Init action
        appStore.waitUntilIdle()
        binding = BrowsingModeBinding(
            appStore,
            themeManager,
            retrieveWindow,
            settings,
            coroutinesTestRule.testDispatcher,
        )
        binding.start()
    }

    @After
    fun teardown() {
        binding.stop()
    }

    @Test
    fun `WHEN mode updated THEN theme manager is also updated`() = runTest {
        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Private)).joinBlocking()

        verify(themeManager).currentTheme = BrowsingMode.Private
    }

    @Test
    fun `GIVEN screenshots not allowed in private mode WHEN mode changes to private THEN secure flag added to window`() = runTestOnMain {
        whenever(window.addFlags(anyInt())).then { }
        whenever(settings.allowScreenshotsInPrivateMode).thenReturn(false)

        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Private)).joinBlocking()

        verify(window).addFlags(WindowManager.LayoutParams.FLAG_SECURE)
    }

    @Test
    fun `GIVEN screenshots allowed in private mode when mode changes to private THEN secure flag not added to window`() = runTest {
        whenever(settings.allowScreenshotsInPrivateMode).thenReturn(true)

        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Private)).joinBlocking()

        verify(window, never()).addFlags(WindowManager.LayoutParams.FLAG_SECURE)
    }

    @Test
    fun `WHEN mode changed to normal THEN secure flag cleared from window`() {
        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Private)).joinBlocking()
        appStore.dispatch(AppAction.ModeChange(BrowsingMode.Normal)).joinBlocking()

        verify(window, times(2)).clearFlags(WindowManager.LayoutParams.FLAG_SECURE)
    }
}
