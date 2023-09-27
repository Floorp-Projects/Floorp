/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.View
import android.widget.Button
import androidx.appcompat.app.AlertDialog
import mozilla.components.browser.state.action.ExtensionProcessDisabledPopupAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
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
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExtensionProcessDisabledControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `WHEN showExtensionProcessDisabledPopup is true AND positive button clicked then enable extension process spawning`() {
        val store = BrowserStore()
        val engine: Engine = mock()
        val dialog: AlertDialog = mock()
        val appName = "TestApp"
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionProcessDisabledController(testContext, store, engine, builder, appName)
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        assertFalse(store.state.showExtensionProcessDisabledPopup)

        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        assertTrue(store.state.showExtensionProcessDisabledPopup)

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder).show()

        buttonsContainerCaptor.value.findViewById<Button>(R.id.positive).performClick()

        store.waitUntilIdle()

        verify(engine).enableExtensionProcessSpawning()
        verify(engine, never()).disableExtensionProcessSpawning()
        assertFalse(store.state.showExtensionProcessDisabledPopup)
        verify(dialog).dismiss()
    }

    @Test
    fun `WHEN showExtensionProcessDisabledPopup is true AND negative button clicked then dismiss without enabling extension process spawning`() {
        val store = BrowserStore()
        val engine: Engine = mock()
        val appName = "TestApp"
        val dialog: AlertDialog = mock()
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionProcessDisabledController(testContext, store, engine, builder, appName)
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        assertFalse(store.state.showExtensionProcessDisabledPopup)

        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        assertTrue(store.state.showExtensionProcessDisabledPopup)

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder).show()

        buttonsContainerCaptor.value.findViewById<Button>(R.id.negative).performClick()

        store.waitUntilIdle()

        assertFalse(store.state.showExtensionProcessDisabledPopup)
        verify(engine, never()).enableExtensionProcessSpawning()
        verify(engine).disableExtensionProcessSpawning()
        verify(dialog).dismiss()
    }

    @Test
    fun `WHEN dispatching the same event twice THEN the dialog should only be created once`() {
        val store = BrowserStore()
        val engine: Engine = mock()
        val appName = "TestApp"
        val dialog: AlertDialog = mock()
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionProcessDisabledController(testContext, store, engine, builder, appName)
        val buttonsContainerCaptor = argumentCaptor<View>()

        controller.start()

        whenever(builder.show()).thenReturn(dialog)

        // First dispatch...
        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        // Second dispatch... without having dismissed the dialog before!
        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(builder).setView(buttonsContainerCaptor.capture())
        verify(builder, times(1)).show()

        // Click a button to dismiss the dialog.
        buttonsContainerCaptor.value.findViewById<Button>(R.id.negative).performClick()
        store.waitUntilIdle()
    }
}
