/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.addons.amo.mozilla.components.feature.addons.update

import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.feature.addons.update.AddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater
import mozilla.components.feature.addons.update.DefaultAddonUpdater.NotificationHandlerService
import mozilla.components.feature.addons.update.DefaultAddonUpdater.UpdateStatusStorage
import mozilla.components.feature.addons.update.GlobalAddonDependencyProvider
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class NotificationHandlerServiceTest {

    @Test
    fun `onHandleIntent - reacts to the allow action`() {

        val allowIntent = Intent(testContext, NotificationHandlerService::class.java).apply {
            action = DefaultAddonUpdater.NOTIFICATION_ACTION_ALLOW
            putExtra(DefaultAddonUpdater.NOTIFICATION_EXTRA_ADDON_ID, "addon_id")
        }

        val handler = spy(NotificationHandlerService())
        val updater = mock<AddonUpdater>()
        val storage = UpdateStatusStorage()

        handler.context = testContext
        GlobalAddonDependencyProvider.initialize(mock(), updater)

        handler.onHandleIntent(allowIntent)

        verify(handler).handleAllowAction(allowIntent)
        verify(handler).removeNotification()
        verify(updater).update("addon_id")
        assertTrue(storage.isPreviouslyAllowed(testContext, "addon_id"))

        storage.clear(testContext)
    }

    @Test
    fun `onHandleIntent - reacts to the deny action`() {

        val allowIntent = Intent(testContext, NotificationHandlerService::class.java).apply {
            action = DefaultAddonUpdater.NOTIFICATION_ACTION_DENY
        }

        val handler = spy(NotificationHandlerService())
        val updater = mock<AddonUpdater>()
        val storage = UpdateStatusStorage()

        handler.context = testContext
        GlobalAddonDependencyProvider.initialize(mock(), updater)

        handler.onHandleIntent(allowIntent)

        verify(handler).removeNotification()
        verify(handler, times(0)).handleAllowAction(allowIntent)
        verify(updater, times(0)).update("addon_id")
        assertFalse(storage.isPreviouslyAllowed(testContext, "addon_id"))
    }
}
