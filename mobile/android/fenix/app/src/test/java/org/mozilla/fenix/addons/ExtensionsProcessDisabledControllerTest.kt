/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.View
import android.widget.Button
import androidx.appcompat.app.AlertDialog
import mozilla.components.browser.state.action.ExtensionsProcessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExtensionsProcessDisabledControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `WHEN showExtensionsProcessDisabledPrompt is true AND positive button clicked then enable extension process spawning`() {
        val browserStore = BrowserStore()
        val dialog: AlertDialog = mock()
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionsProcessDisabledController(
            context = testContext,
            appStore = AppStore(AppState(isForeground = true)),
            browserStore = browserStore,
            builder = builder,
            appName = "TestApp",
        )
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        assertFalse(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertFalse(browserStore.state.extensionsProcessDisabled)

        // Pretend the process has been disabled and we show the dialog.
        browserStore.dispatch(ExtensionsProcessAction.DisabledAction)
        browserStore.dispatch(ExtensionsProcessAction.ShowPromptAction(show = true))
        dispatcher.scheduler.advanceUntilIdle()
        browserStore.waitUntilIdle()
        assertTrue(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertTrue(browserStore.state.extensionsProcessDisabled)

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder).show()

        buttonsContainerCaptor.value.findViewById<Button>(R.id.positive).performClick()

        browserStore.waitUntilIdle()

        assertFalse(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertFalse(browserStore.state.extensionsProcessDisabled)
        verify(dialog).dismiss()
    }

    @Test
    fun `WHEN showExtensionsProcessDisabledPrompt is true AND negative button clicked then dismiss without enabling extension process spawning`() {
        val browserStore = BrowserStore()
        val dialog: AlertDialog = mock()
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionsProcessDisabledController(
            context = testContext,
            appStore = AppStore(AppState(isForeground = true)),
            browserStore = browserStore,
            builder = builder,
            appName = "TestApp",
        )
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        assertFalse(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertFalse(browserStore.state.extensionsProcessDisabled)

        // Pretend the process has been disabled and we show the dialog.
        browserStore.dispatch(ExtensionsProcessAction.DisabledAction)
        browserStore.dispatch(ExtensionsProcessAction.ShowPromptAction(show = true))
        dispatcher.scheduler.advanceUntilIdle()
        browserStore.waitUntilIdle()
        assertTrue(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertTrue(browserStore.state.extensionsProcessDisabled)

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder).show()

        buttonsContainerCaptor.value.findViewById<Button>(R.id.negative).performClick()

        browserStore.waitUntilIdle()

        assertFalse(browserStore.state.showExtensionsProcessDisabledPrompt)
        assertTrue(browserStore.state.extensionsProcessDisabled)
        verify(dialog).dismiss()
    }

    @Test
    fun `WHEN dispatching the same event twice THEN the dialog should only be created once`() {
        val browserStore = BrowserStore()
        val dialog: AlertDialog = mock()
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionsProcessDisabledController(
            context = testContext,
            appStore = AppStore(AppState(isForeground = true)),
            browserStore = browserStore,
            builder = builder,
            appName = "TestApp",
        )
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        // First dispatch...
        browserStore.dispatch(ExtensionsProcessAction.ShowPromptAction(show = true))
        dispatcher.scheduler.advanceUntilIdle()
        browserStore.waitUntilIdle()

        // Second dispatch... without having dismissed the dialog before!
        browserStore.dispatch(ExtensionsProcessAction.ShowPromptAction(show = true))
        dispatcher.scheduler.advanceUntilIdle()
        browserStore.waitUntilIdle()

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder, times(1)).show()

        // Click a button to dismiss the dialog.
        buttonsContainerCaptor.value.findViewById<Button>(R.id.negative).performClick()
        browserStore.waitUntilIdle()
    }

    @Test
    fun `WHEN app is backgrounded AND extension process spawning threshold is exceeded THEN kill the app`() {
        val browserStore = BrowserStore(BrowserState())
        val appStore = AppStore(AppState(isForeground = false))
        val builder: AlertDialog.Builder = mock()
        val appName = "TestApp"
        val onKillApp: () -> Unit = mock()

        val controller = ExtensionsProcessDisabledController(testContext, browserStore, appStore, builder, appName, onKillApp)

        controller.start()

        browserStore.dispatch(ExtensionsProcessAction.ShowPromptAction(show = true))
        dispatcher.scheduler.advanceUntilIdle()
        browserStore.waitUntilIdle()

        verify(onKillApp).invoke()
    }
}
