/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.content.Context
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.window.WindowRequest
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CustomTabWindowFeatureTest {

    private val testDispatcher = TestCoroutineDispatcher()

    private lateinit var store: BrowserStore
    private val sessionId = "session-uuid"
    private lateinit var context: Context

    @Before
    fun setup() {
        context = mock()

        Dispatchers.setMain(testDispatcher)
        store = spy(BrowserStore(BrowserState(
            customTabs = listOf(
                createCustomTab(id = sessionId, url = "https://www.mozilla.org")
            )
        )))

        whenever(context.packageName).thenReturn("org.mozilla.firefox")
    }

    @Test
    fun `handles request to open window`() {
        val feature = CustomTabWindowFeature(context, store, sessionId)
        feature.start()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")
        store.dispatch(ContentAction.UpdateWindowRequestAction(sessionId, windowRequest)).joinBlocking()
        verify(context).startActivity(any(), any())
        verify(store).dispatch(ContentAction.ConsumeWindowRequestAction(sessionId))
    }

    @Test
    fun `creates intent based on default custom tab config`() {
        val feature = CustomTabWindowFeature(context, store, sessionId)
        val config = CustomTabConfig()
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEqualConfigs(config, newConfig)
    }

    @Test
    fun `creates intent based on custom tab config`() {
        val feature = CustomTabWindowFeature(context, store, sessionId)
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
        val feature = CustomTabWindowFeature(context, store, sessionId)
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
    fun `handles no requests when stopped`() {
        val feature = CustomTabWindowFeature(context, store, sessionId)
        feature.start()
        feature.stop()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")
        store.dispatch(ContentAction.UpdateWindowRequestAction(sessionId, windowRequest)).joinBlocking()
        verify(context, never()).startActivity(any(), any())
        verify(store, never()).dispatch(ContentAction.ConsumeWindowRequestAction(sessionId))
    }

    private fun assertEqualConfigs(expected: CustomTabConfig, actual: CustomTabConfig) {
        assertEquals(expected.copy(id = ""), actual.copy(id = ""))
    }
}
