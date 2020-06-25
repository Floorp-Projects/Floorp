/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.Context
import io.mockk.Called
import io.mockk.MockKAnnotations
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class NotificationSessionObserverTest {

    private lateinit var observer: NotificationSessionObserver
    private lateinit var store: BrowserStore
    @MockK private lateinit var context: Context
    @MockK(relaxed = true) private lateinit var notificationService: SessionNotificationService.Companion

    @Before
    fun before() {
        MockKAnnotations.init(this)
        store = BrowserStore()
        every { context.components.core.store } returns store
        observer = NotificationSessionObserver(context, notificationService)
        NotificationSessionObserver.isStartedFromPrivateShortcut = false
    }

    @Test
    fun `GIVEN session is private and non-custom WHEN it is added THEN notification service should be started`() = runBlocking {
        val privateSession = createTab("https://firefox.com", private = true)

        store.dispatch(TabListAction.AddTabAction(privateSession)).join()

        observer.start()
        verify(exactly = 1) { notificationService.start(context, false) }
        confirmVerified(notificationService)
    }

    @Test
    fun `GIVEN session is not private WHEN it is added THEN notification service should not be started`() = runBlocking {
        val normalSession = createTab("https://firefox.com")
        val customSession = createCustomTab("https://firefox.com")

        observer.start()
        verify { notificationService wasNot Called }

        store.dispatch(TabListAction.AddTabAction(normalSession)).join()
        verify(exactly = 0) { notificationService.start(context, false) }

        store.dispatch(CustomTabListAction.AddCustomTabAction(customSession)).join()
        verify(exactly = 0) { notificationService.start(context, false) }
    }

    @Test
    fun `GIVEN session is custom tab WHEN it is added THEN notification service should not be started`() = runBlocking {
        val privateCustomSession = createCustomTab("https://firefox.com").let {
            it.copy(content = it.content.copy(private = true))
        }
        val customSession = createCustomTab("https://firefox.com")

        observer.start()
        verify { notificationService wasNot Called }

        store.dispatch(CustomTabListAction.AddCustomTabAction(privateCustomSession)).join()
        verify(exactly = 0) { notificationService.start(context, false) }

        store.dispatch(CustomTabListAction.AddCustomTabAction(customSession)).join()
        verify(exactly = 0) { notificationService.start(context, false) }
    }
}
