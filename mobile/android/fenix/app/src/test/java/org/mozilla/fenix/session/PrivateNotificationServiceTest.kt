/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.ComponentName
import android.content.Intent
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.privatemode.notification.AbstractPrivateNotificationService.Companion.ACTION_ERASE
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.HomeActivity.Companion.PRIVATE_BROWSING_MODE
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import org.robolectric.android.controller.ServiceController

@RunWith(FenixRobolectricTestRunner::class)
class PrivateNotificationServiceTest {

    private lateinit var controller: ServiceController<PrivateNotificationService>
    private lateinit var store: BrowserStore

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Before
    fun setup() {
        store = mockk()
        every { store.dispatch(any()) } returns mockk()
        every { testContext.components.core.store } returns store
        every { testContext.components.useCases.tabsUseCases } returns TabsUseCases(store)

        controller = Robolectric.buildService(
            PrivateNotificationService::class.java,
            Intent(ACTION_ERASE),
        )
    }

    @Test
    fun `service opens home activity in private mode if app is in private mode`() {
        val selectedPrivateTab = createTab("https://mozilla.org", private = true)
        every { store.state } returns BrowserState(tabs = listOf(selectedPrivateTab), selectedTabId = selectedPrivateTab.id)

        val service = shadowOf(controller.get())
        controller.startCommand(0, 0)

        val intent = service.nextStartedActivity
        assertNotNull(intent)
        assertEquals(ComponentName(testContext, HomeActivity::class.java), intent.component)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK, intent.flags)
        assertEquals(true, intent.extras?.getBoolean(PRIVATE_BROWSING_MODE))
    }

    @Test
    fun `service starts no activity if app is in normal mode`() {
        val selectedPrivateTab = createTab("https://mozilla.org", private = false)
        every { store.state } returns BrowserState(tabs = listOf(selectedPrivateTab), selectedTabId = selectedPrivateTab.id)

        val service = shadowOf(controller.get())
        controller.startCommand(0, 0)

        assertNull(service.nextStartedActivity)
    }
}
