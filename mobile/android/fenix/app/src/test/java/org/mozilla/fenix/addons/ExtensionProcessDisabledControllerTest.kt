/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.content.Context
import android.content.DialogInterface.OnClickListener
import androidx.appcompat.app.AlertDialog
import mozilla.components.browser.state.action.ExtensionProcessDisabledPopupAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.anyString
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class ExtensionProcessDisabledControllerTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun `WHEN showExtensionProcessDisabledPopup is true AND positive button clicked then enable extension process spawning`() {
        val context: Context = mock()
        val store = BrowserStore()
        val engine: Engine = mock()
        val appName = "TestApp"
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionProcessDisabledController(context, store, engine, builder, appName)
        controller.start()

        whenever(context.getString(anyInt(), anyString())).thenReturn("TestString")

        val posClickCaptor = argumentCaptor<OnClickListener>()

        assertFalse(store.state.showExtensionProcessDisabledPopup)

        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        assertTrue(store.state.showExtensionProcessDisabledPopup)

        verify(builder).setPositiveButton(anyInt(), posClickCaptor.capture())

        verify(builder).show()

        val dialog: AlertDialog = mock()
        posClickCaptor.value.onClick(dialog, 1)
        store.waitUntilIdle()

        verify(engine).enableExtensionProcessSpawning()
        assertFalse(store.state.showExtensionProcessDisabledPopup)
        verify(dialog).dismiss()
    }

    @Test
    fun `WHEN showExtensionProcessDisabledPopup is true AND negative button clicked then dismiss without enabling extension process spawning`() {
        val context: Context = mock()
        val store = BrowserStore()
        val engine: Engine = mock()
        val appName = "TestApp"
        val builder: AlertDialog.Builder = mock()
        val controller = ExtensionProcessDisabledController(context, store, engine, builder, appName)
        controller.start()

        whenever(context.getString(anyInt(), anyString())).thenReturn("TestString")

        val negClickCaptor = argumentCaptor<OnClickListener>()

        assertFalse(store.state.showExtensionProcessDisabledPopup)

        store.dispatch(ExtensionProcessDisabledPopupAction(showPopup = true))
        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()
        assertTrue(store.state.showExtensionProcessDisabledPopup)

        verify(builder).setNegativeButton(anyInt(), negClickCaptor.capture())

        verify(builder).show()

        val dialog: AlertDialog = mock()
        negClickCaptor.value.onClick(dialog, 1)
        store.waitUntilIdle()

        assertFalse(store.state.showExtensionProcessDisabledPopup)
        verify(engine, never()).enableExtensionProcessSpawning()
        verify(dialog).dismiss()
    }
}
