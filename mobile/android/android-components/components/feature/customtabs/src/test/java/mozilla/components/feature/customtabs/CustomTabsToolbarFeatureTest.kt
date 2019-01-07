/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.graphics.Color
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.anyInt
import org.mockito.Mockito.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CustomTabsToolbarFeatureTest {

    @Test
    fun `start with no sessionId invokes nothing`() {
        val sessionManager: SessionManager = spy(SessionManager(mock()))
        val session: Session = mock()

        val feature = spy(CustomTabsToolbarFeature(sessionManager, mock()))

        feature.start()

        verify(sessionManager, never()).findSessionById(anyString())
        verify(feature, never()).initialize(session)
    }

    @Test
    fun `start calls initialize with the sessionId`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        `when`(sessionManager.findSessionById(anyString())).thenReturn(session)

        val feature = spy(CustomTabsToolbarFeature(sessionManager, mock(), ""))

        feature.start()

        verify(feature).initialize(session)
    }

    @Test
    fun `initialize updates toolbar`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(sessionManager, mock(), ""))

        feature.initialize(session)

        verify(feature).updateToolbarColor(anyInt())
    }

    @Test
    fun `updateToolbarColor changes background and textColor`() {
        val sessionManager: SessionManager = mock()
        val session: Session = mock()
        val toolbar: BrowserToolbar = mock()
        `when`(session.customTabConfig).thenReturn(mock())

        val feature = spy(CustomTabsToolbarFeature(sessionManager, toolbar, ""))

        feature.updateToolbarColor(null)

        verify(toolbar, never()).setBackgroundColor(anyInt())
        verify(toolbar, never()).textColor = anyInt()

        feature.updateToolbarColor(123)

        verify(toolbar).setBackgroundColor(anyInt())
        verify(toolbar).textColor = anyInt()
    }

    @Test
    fun getReadableTextColor() {
        // White text color for a black background
        val white = CustomTabsToolbarFeature.getReadableTextColor(0)
        assertEquals(Color.WHITE, white)

        // Black text color for a white background
        val black = CustomTabsToolbarFeature.getReadableTextColor(0xFFFFFF)
        assertEquals(Color.BLACK, black)
    }
}