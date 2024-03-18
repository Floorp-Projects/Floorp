/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.readermode

import android.content.res.ColorStateList
import android.view.View
import android.widget.Button
import android.widget.RadioButton
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultReaderModeControllerTest {

    private lateinit var readerViewFeature: ReaderViewFeature
    private lateinit var featureWrapper: ViewBoundFeatureWrapper<ReaderViewFeature>
    private lateinit var readerViewControlsBar: View
    private lateinit var onReaderModeChanged: () -> Unit

    @Before
    fun setup() {
        val tab = createTab("https://mozilla.org")
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id,
            ),
        )
        readerViewFeature = spyk(ReaderViewFeature(testContext, mockk(), store, mockk()))

        featureWrapper = ViewBoundFeatureWrapper(
            feature = readerViewFeature,
            owner = mockk(relaxed = true),
            view = mockk(relaxed = true),
        )
        readerViewControlsBar = mockk(relaxed = true)
        onReaderModeChanged = mockk(relaxed = true)

        every { readerViewFeature.hideReaderView() } returns Unit
        every { readerViewFeature.showReaderView() } returns Unit
        every { readerViewFeature.showControls() } returns Unit
        every { readerViewFeature.hideControls() } returns Unit
    }

    @Test
    fun testHideReaderView() {
        val controller = DefaultReaderModeController(
            featureWrapper,
            readerViewControlsBar,
            onReaderModeChanged = onReaderModeChanged,
        )
        controller.hideReaderView()
        verify { readerViewFeature.hideReaderView() }
        verify { readerViewFeature.hideControls() }
        verify { onReaderModeChanged.invoke() }
    }

    @Test
    fun testShowReaderView() {
        val controller = DefaultReaderModeController(
            featureWrapper,
            readerViewControlsBar,
            onReaderModeChanged = onReaderModeChanged,
        )
        controller.showReaderView()
        verify { readerViewFeature.showReaderView() }
        verify { onReaderModeChanged.invoke() }
    }

    @Test
    fun testShowControlsNormalTab() {
        val controller = DefaultReaderModeController(
            featureWrapper,
            readerViewControlsBar,
            isPrivate = false,
        )

        controller.showControls()
        verify { readerViewFeature.showControls() }
        verify { readerViewControlsBar wasNot Called }
    }

    @Test
    fun testShowControlsPrivateTab() {
        val controller = spyk(
            DefaultReaderModeController(
                featureWrapper,
                readerViewControlsBar,
                isPrivate = true,
            ),
        )

        val privateButtonColor = mockk<ColorStateList>()
        val privateRadioButtonColor = mockk<ColorStateList>()

        every { controller.privateButtonColor } returns privateButtonColor
        every { controller.privateRadioButtonColor } returns privateRadioButtonColor

        val decrease = mockk<Button>(relaxUnitFun = true)
        val increase = mockk<Button>(relaxUnitFun = true)
        val serif = mockk<RadioButton>(relaxUnitFun = true)
        val sansSerif = mockk<RadioButton>(relaxUnitFun = true)

        every {
            readerViewControlsBar.findViewById<Button>(R.id.mozac_feature_readerview_font_size_decrease)
        } returns decrease
        every {
            readerViewControlsBar.findViewById<Button>(R.id.mozac_feature_readerview_font_size_increase)
        } returns increase
        every {
            readerViewControlsBar.findViewById<RadioButton>(R.id.mozac_feature_readerview_font_serif)
        } returns serif
        every {
            readerViewControlsBar.findViewById<RadioButton>(R.id.mozac_feature_readerview_font_sans_serif)
        } returns sansSerif

        controller.showControls()
        verify { readerViewFeature.showControls() }
        verifyAll {
            decrease.setTextColor(privateButtonColor)
            increase.setTextColor(privateButtonColor)
            serif.setTextColor(privateRadioButtonColor)
            sansSerif.setTextColor(privateRadioButtonColor)
        }
    }
}
