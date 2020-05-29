/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.readermode

import android.view.View
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.readerview.ReaderViewFeature
import mozilla.components.support.base.feature.ViewBoundFeatureWrapper
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultReaderModeControllerTest {

    private lateinit var readerViewFeature: ReaderViewFeature
    private lateinit var featureWrapper: ViewBoundFeatureWrapper<ReaderViewFeature>
    private lateinit var readerViewControlsBar: View

    @Before
    fun setup() {
        val tab = createTab("https://mozilla.org")
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                selectedTabId = tab.id
            )
        )
        readerViewFeature = spyk(ReaderViewFeature(testContext, mockk(), store, mockk()))

        featureWrapper = ViewBoundFeatureWrapper(
            feature = readerViewFeature,
            owner = mockk(relaxed = true),
            view = mockk(relaxed = true)
        )
        readerViewControlsBar = mockk(relaxed = true)

        every { readerViewFeature.hideReaderView() } returns Unit
        every { readerViewFeature.showReaderView() } returns Unit
        every { readerViewFeature.showControls() } returns Unit
    }

    @Test
    fun testHideReaderView() {
        val controller = DefaultReaderModeController(featureWrapper, readerViewControlsBar)
        controller.hideReaderView()
        verify { readerViewFeature.hideReaderView() }
    }

    @Test
    fun testShowReaderView() {
        val controller = DefaultReaderModeController(featureWrapper, readerViewControlsBar)
        controller.showReaderView()
        verify { readerViewFeature.showReaderView() }
    }

    @Test
    fun testShowControlsNormalTab() {
        val controller = DefaultReaderModeController(
            featureWrapper,
            readerViewControlsBar,
            isPrivate = false
        )

        controller.showControls()
        verify { readerViewFeature.showControls() }
        verify { readerViewControlsBar wasNot Called }
    }
}
