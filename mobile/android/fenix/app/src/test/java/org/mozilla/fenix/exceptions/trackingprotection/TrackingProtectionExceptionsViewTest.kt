/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import android.text.Spannable
import android.text.method.LinkMovementMethod
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.core.view.isVisible
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkConstructor
import io.mockk.unmockkConstructor
import io.mockk.verify
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.ComponentExceptionsBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionExceptionsViewTest {

    private lateinit var container: ViewGroup
    private lateinit var interactor: TrackingProtectionExceptionsInteractor
    private lateinit var exceptionsView: TrackingProtectionExceptionsView
    private lateinit var binding: ComponentExceptionsBinding

    @Before
    fun setup() {
        mockkConstructor(TrackingProtectionExceptionsAdapter::class)
        every { anyConstructed<TrackingProtectionExceptionsAdapter>().updateData(any()) } just Runs

        container = FrameLayout(testContext)
        interactor = mockk()

        exceptionsView = TrackingProtectionExceptionsView(
            container,
            interactor
        )
        binding = ComponentExceptionsBinding.bind(container)
    }

    @After
    fun teardown() {
        unmockkConstructor(TrackingProtectionExceptionsAdapter::class)
    }

    @Test
    fun `binds exception text`() {
        assertTrue(binding.exceptionsLearnMore.movementMethod is LinkMovementMethod)
        assertTrue(binding.exceptionsLearnMore.text is Spannable)
        assertEquals("Learn more", binding.exceptionsLearnMore.text.toString())

        every { interactor.onLearnMore() } just Runs
        binding.exceptionsLearnMore.performClick()
        verify { interactor.onLearnMore() }
    }

    @Test
    fun `binds empty list to adapter`() {
        exceptionsView.update(emptyList())

        assertTrue(binding.exceptionsEmptyView.isVisible)
        assertFalse(binding.exceptionsList.isVisible)

        verify { anyConstructed<TrackingProtectionExceptionsAdapter>().updateData(emptyList()) }
    }

    @Test
    fun `binds list with items to adapter`() {
        val items = listOf<TrackingProtectionException>(mockk(), mockk())
        exceptionsView.update(items)

        assertFalse(binding.exceptionsEmptyView.isVisible)
        assertTrue(binding.exceptionsList.isVisible)
        verify { anyConstructed<TrackingProtectionExceptionsAdapter>().updateData(items) }
    }
}
