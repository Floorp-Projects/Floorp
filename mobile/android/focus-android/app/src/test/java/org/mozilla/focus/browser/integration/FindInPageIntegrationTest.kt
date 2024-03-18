/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import androidx.core.view.isVisible
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.feature.findinpage.view.FindInPageBar
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.spy

internal class FindInPageIntegrationTest {
    // For ease of tests naming "find in page bar" is referred to as FIPB.
    val toolbar: BrowserToolbar = mock()
    val findInPageBar: FindInPageBar = mock()

    @Test
    fun `GIVEN FIPB not shown WHEN show is called THEN FIPB is shown`() {
        val feature = spy(FindInPageIntegration(mock(), findInPageBar, toolbar, mock()))
        val sessionState: SessionState = mock()
        val contentState: ContentState = mock()
        val engineState: EngineState = mock()
        whenever(sessionState.content).thenReturn(contentState)
        whenever(sessionState.engineState).thenReturn(engineState)

        feature.show(sessionState)

        Mockito.verify(findInPageBar).isVisible = true
    }

    @Test
    fun `GIVEN FIPB is shown WHEN hide is called THEN FIPB is hidden`() {
        val feature = spy(FindInPageIntegration(mock(), findInPageBar, toolbar, mock()))

        feature.hide()

        Mockito.verify(findInPageBar).isVisible = false
    }
}
