/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.Activity
import android.content.ActivityNotFoundException
import android.graphics.Color
import android.net.Uri
import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
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
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class CustomTabWindowFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var store: BrowserStore
    private val sessionId = "session-uuid"
    private lateinit var activity: Activity
    private val launchUrlFallback: (Uri) -> Unit = spy { _ -> }

    @Before
    fun setup() {
        activity = mock()

        store = spy(
            BrowserStore(
                BrowserState(
                    customTabs = listOf(
                        createCustomTab(id = sessionId, url = "https://www.mozilla.org"),
                    ),
                ),
            ),
        )

        whenever(activity.packageName).thenReturn("org.mozilla.firefox")
    }

    @Test
    fun `given a request to open window, when the url can be handled, then the activity should start`() {
        val feature = spy(CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback))
        val windowRequest: WindowRequest = mock()

        feature.start()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")
        store.dispatch(ContentAction.UpdateWindowRequestAction(sessionId, windowRequest)).joinBlocking()

        verify(activity).startActivity(any(), any())
        verify(store).dispatch(ContentAction.ConsumeWindowRequestAction(sessionId))
    }

    @Test
    fun `given a request to open window, when the url can't be handled, then handleError should be called`() {
        val exception = ActivityNotFoundException()
        val feature = spy(CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback))
        val windowRequest: WindowRequest = mock()

        feature.start()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("blob:https://www.firefox.com")
        whenever(activity.startActivity(any(), any())).thenThrow(exception)
        store.dispatch(ContentAction.UpdateWindowRequestAction(sessionId, windowRequest)).joinBlocking()

        verify(launchUrlFallback).invoke("blob:https://www.firefox.com".toUri())
    }

    @Test
    fun `creates intent based on default custom tab config`() {
        val feature = CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback)
        val config = CustomTabConfig()
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEquals(config, newConfig)
    }

    @Test
    fun `creates intent based on custom tab config`() {
        val feature = CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback)
        val config = CustomTabConfig(
            toolbarColor = Color.RED,
            navigationBarColor = Color.BLUE,
            enableUrlbarHiding = true,
            showShareMenuItem = true,
            titleVisible = true,
        )
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEquals(config, newConfig)
    }

    @Test
    fun `creates intent with same menu items`() {
        val feature = CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback)
        val config = CustomTabConfig(
            actionButtonConfig = CustomTabActionButtonConfig(
                description = "button",
                icon = mock(),
                pendingIntent = mock(),
            ),
            menuItems = listOf(
                CustomTabMenuItem("Item A", mock()),
                CustomTabMenuItem("Item B", mock()),
                CustomTabMenuItem("Item C", mock()),
            ),
        )
        val intent = feature.configToIntent(config)

        val newConfig = createCustomTabConfigFromIntent(intent.intent, null)
        assertEquals("org.mozilla.firefox", intent.intent.`package`)
        assertEquals(config, newConfig)
    }

    @Test
    fun `handles no requests when stopped`() {
        val feature = CustomTabWindowFeature(activity, store, sessionId, launchUrlFallback)
        feature.start()
        feature.stop()

        val windowRequest: WindowRequest = mock()
        whenever(windowRequest.type).thenReturn(WindowRequest.Type.OPEN)
        whenever(windowRequest.url).thenReturn("https://www.firefox.com")
        store.dispatch(ContentAction.UpdateWindowRequestAction(sessionId, windowRequest)).joinBlocking()
        verify(activity, never()).startActivity(any(), any())
        verifyNoInteractions(launchUrlFallback)
        verify(store, never()).dispatch(ContentAction.ConsumeWindowRequestAction(sessionId))
    }
}
