/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import android.content.ComponentName
import android.content.Intent
import io.mockk.every
import io.mockk.mockk
import mozilla.components.feature.privatemode.notification.AbstractPrivateNotificationService.Companion.ACTION_ERASE
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
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

    @Before
    fun setup() {
        val store = testContext.components.core.store
        every { store.dispatch(any()) } returns mockk()

        controller = Robolectric.buildService(
            PrivateNotificationService::class.java,
            Intent(ACTION_ERASE)
        )
    }

    @Test
    fun `service opens home activity with PBM flag set to true`() {
        PrivateNotificationService.isStartedFromPrivateShortcut = true
        val service = shadowOf(controller.get())
        controller.startCommand(0, 0)

        val intent = service.nextStartedActivity
        assertEquals(ComponentName(testContext, HomeActivity::class.java), intent.component)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK, intent.flags)
        assertEquals(true, intent.extras?.getBoolean(PRIVATE_BROWSING_MODE))
    }

    @Test
    fun `service opens home activity with PBM flag set to false`() {
        PrivateNotificationService.isStartedFromPrivateShortcut = false
        val service = shadowOf(controller.get())
        controller.startCommand(0, 0)

        val intent = service.nextStartedActivity
        assertEquals(ComponentName(testContext, HomeActivity::class.java), intent.component)
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK, intent.flags)
        assertEquals(false, intent.extras?.getBoolean(PRIVATE_BROWSING_MODE))
    }
}
