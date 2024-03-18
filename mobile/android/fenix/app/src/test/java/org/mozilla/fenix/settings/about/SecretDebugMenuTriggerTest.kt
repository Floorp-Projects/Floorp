/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.about

import android.content.Context
import android.view.View
import android.widget.Toast
import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

class SecretDebugMenuTriggerTest {

    @MockK private lateinit var logoView: View

    @MockK private lateinit var context: Context

    @MockK private lateinit var settings: Settings

    @MockK(relaxUnitFun = true)
    private lateinit var toast: Toast
    private lateinit var clickListener: CapturingSlot<View.OnClickListener>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkStatic(Toast::class)
        clickListener = slot()

        every { logoView.setOnClickListener(capture(clickListener)) } just Runs
        every { logoView.context } returns context
        every {
            context.getString(R.string.about_debug_menu_toast_progress, any())
        } returns "Debug menu: x click(s) left to enable"
        every { settings.showSecretDebugMenuThisSession } returns false
        every { settings.showSecretDebugMenuThisSession = any() } just Runs
        every { Toast.makeText(context, any<Int>(), any()) } returns toast
        every { Toast.makeText(context, any<String>(), any()) } returns toast
    }

    @After
    fun teardown() {
        unmockkStatic(Toast::class)
    }

    @Test
    fun `toast is not displayed on first click`() {
        SecretDebugMenuTrigger(logoView, settings)
        clickListener.captured.onClick(logoView)

        verify(inverse = true) { Toast.makeText(context, any<String>(), any()) }
        verify(inverse = true) { toast.show() }
    }

    @Test
    fun `toast is displayed on second click`() {
        SecretDebugMenuTrigger(logoView, settings)
        clickListener.captured.onClick(logoView)
        clickListener.captured.onClick(logoView)

        verify { context.getString(R.string.about_debug_menu_toast_progress, 3) }
        verify { Toast.makeText(context, any<String>(), Toast.LENGTH_SHORT) }
        verify { toast.show() }
    }

    @Test
    fun `clearClickCounter resets counter`() {
        val trigger = SecretDebugMenuTrigger(logoView, settings)

        clickListener.captured.onClick(logoView)
        trigger.onResume(mockk())

        clickListener.captured.onClick(logoView)

        verify(inverse = true) { Toast.makeText(context, any<String>(), any()) }
        verify(inverse = true) { toast.show() }
    }

    @Test
    fun `toast is displayed on fifth click`() {
        SecretDebugMenuTrigger(logoView, settings)
        clickListener.captured.onClick(logoView)
        clickListener.captured.onClick(logoView)
        clickListener.captured.onClick(logoView)
        clickListener.captured.onClick(logoView)
        clickListener.captured.onClick(logoView)

        verify {
            Toast.makeText(
                context,
                R.string.about_debug_menu_toast_done,
                Toast.LENGTH_LONG,
            )
        }
        verify { toast.show() }
        verify { settings.showSecretDebugMenuThisSession = true }
    }

    @Test
    fun `don't register click listener if menu is already shown`() {
        every { settings.showSecretDebugMenuThisSession } returns true
        SecretDebugMenuTrigger(logoView, settings)

        verify(inverse = true) { logoView.setOnClickListener(any()) }
    }
}
