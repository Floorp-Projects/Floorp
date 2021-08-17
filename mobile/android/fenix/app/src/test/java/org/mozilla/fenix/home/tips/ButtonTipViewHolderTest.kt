/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.tips

import android.content.SharedPreferences
import android.view.LayoutInflater
import androidx.core.view.isGone
import androidx.core.view.isVisible
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.components.tips.TipType
import org.mozilla.fenix.databinding.ButtonTipItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.SessionControlInteractor
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class ButtonTipViewHolderTest {

    @MockK private lateinit var activity: HomeActivity
    @MockK private lateinit var interactor: SessionControlInteractor
    @MockK(relaxed = true) private lateinit var metrics: MetricController
    @MockK private lateinit var settings: Settings
    @MockK private lateinit var sharedPrefs: SharedPreferences
    @MockK private lateinit var sharedPrefsEditor: SharedPreferences.Editor
    private lateinit var viewHolder: ButtonTipViewHolder
    private lateinit var binding: ButtonTipItemBinding

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        val view = spyk(
            LayoutInflater.from(testContext)
                .inflate(ButtonTipViewHolder.LAYOUT_ID, null)
        )

        viewHolder = ButtonTipViewHolder(view, interactor, metrics, settings)
        binding = ButtonTipItemBinding.bind(view)
        every { view.context } returns activity
        every { activity.openToBrowserAndLoad(any(), any(), any()) } just Runs
        every { interactor.onCloseTip(any()) } just Runs
        every { settings.preferences } returns sharedPrefs
        every { sharedPrefs.edit() } returns sharedPrefsEditor
        every { sharedPrefsEditor.putBoolean(any(), any()) } returns sharedPrefsEditor
        every { sharedPrefsEditor.apply() } just Runs
    }

    @Test
    fun `text is displayed based on given tip`() {
        viewHolder.bind(defaultTip())

        assertEquals("Tip Title", binding.tipHeaderText.text)
        assertEquals("Tip description", binding.tipDescriptionText.text)
        assertEquals("button", binding.tipButton.text)

        verify { metrics.track(Event.TipDisplayed("tipIdentifier")) }
    }

    @Test
    fun `learn more is hidden if learnMoreURL is null`() {
        viewHolder.bind(defaultTip(learnMoreUrl = null))

        assertTrue(binding.tipLearnMore.isGone)
    }

    @Test
    fun `learn more is visible if learnMoreURL is not null`() {
        viewHolder.bind(defaultTip(learnMoreUrl = "https://learnmore.com"))

        assertTrue(binding.tipLearnMore.isVisible)

        binding.tipLearnMore.performClick()
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = "https://learnmore.com",
                newTab = true,
                from = BrowserDirection.FromHome
            )
        }
    }

    @Test
    fun `tip button invokes tip action`() {
        val action = mockk<() -> Unit>(relaxed = true)
        viewHolder.bind(defaultTip(action))

        binding.tipButton.performClick()
        verify { action() }
        verify { metrics.track(Event.TipPressed("tipIdentifier")) }
    }

    @Test
    fun `close button invokes onCloseTip`() {
        val tip = defaultTip()
        viewHolder.bind(tip)

        binding.tipClose.performClick()
        verify { interactor.onCloseTip(tip) }
        verify { metrics.track(Event.TipClosed("tipIdentifier")) }
        verify { sharedPrefsEditor.putBoolean("tipIdentifier", false) }
    }

    private fun defaultTip(
        action: () -> Unit = mockk(),
        learnMoreUrl: String? = null
    ) = Tip(
        type = TipType.Button("button", action),
        identifier = "tipIdentifier",
        title = "Tip Title",
        description = "Tip description",
        learnMoreURL = learnMoreUrl
    )
}
