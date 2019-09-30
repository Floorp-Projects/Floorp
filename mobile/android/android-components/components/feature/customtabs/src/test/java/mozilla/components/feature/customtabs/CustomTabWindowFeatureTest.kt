/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Context
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CustomTabWindowFeatureTest {

    private val sessionId = "session-uuid"
    private lateinit var context: Context
    private lateinit var engine: Engine
    private lateinit var sessionManager: SessionManager
    private lateinit var session: Session

    @Before
    fun setup() {
        context = mock()
        engine = mock()
        sessionManager = mock()
        session = mock()

        whenever(context.packageName).thenReturn("org.mozilla.firefox")
    }

    @Test
    fun `start registers window observer`() {
        whenever(sessionManager.findSessionById(sessionId)).thenReturn(session)

        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        feature.start()
        verify(session).register(feature.windowObserver)
    }

    @Test
    fun `observer handles open window request`() {
        whenever(sessionManager.findSessionById(sessionId)).thenReturn(session)

        val session = Session("https://www.mozilla.org")
        val request = mock<WindowRequest>()
        whenever(request.url).thenReturn("about:blank")

        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        feature.windowObserver.onOpenWindowRequested(session, request)

        verify(context).startActivity(any(), any())
    }

    @Test
    fun `creates intent based on default custom tab config`() {
        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        val config = CustomTabConfig()
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEqualConfigs(config, newConfig)
    }

    @Test
    fun `creates intent based on custom tab config`() {
        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        val config = CustomTabConfig(
            toolbarColor = Color.RED,
            navigationBarColor = Color.BLUE,
            enableUrlbarHiding = true,
            showShareMenuItem = true,
            titleVisible = true
        )
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEqualConfigs(config, newConfig)
    }

    @Test
    fun `creates intent with same menu items`() {
        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        val config = CustomTabConfig(
            actionButtonConfig = CustomTabActionButtonConfig(
                description = "button",
                icon = mock(),
                pendingIntent = mock()
            ),
            menuItems = listOf(
                CustomTabMenuItem("Item A", mock()),
                CustomTabMenuItem("Item B", mock()),
                CustomTabMenuItem("Item C", mock())
            )
        )
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEqualConfigs(config, newConfig)
    }

    @Test
    fun `stop unregisters window observer`() {
        whenever(sessionManager.findSessionById(sessionId)).thenReturn(session)

        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        feature.stop()
        verify(session).unregister(feature.windowObserver)
    }

    @Test
    fun `start and stop are no-op when session is null`() {
        whenever(sessionManager.findSessionById(sessionId)).thenReturn(null)

        val feature = CustomTabWindowFeature(context, sessionManager, sessionId)
        feature.start()
        feature.stop()
        verify(session, never()).register(feature.windowObserver)
        verify(session, never()).unregister(feature.windowObserver)
    }

    private fun assertEqualConfigs(expected: CustomTabConfig, actual: CustomTabConfig) {
        assertEquals(expected.copy(id = ""), actual.copy(id = ""))
    }
}
