/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class QuickSettingsInteractorTest {
    private val controller = mockk<QuickSettingsController>(relaxed = true)
    private val interactor = QuickSettingsInteractor(controller)

    @Test
    fun `onPermissionsShown should delegate the controller`() {
        interactor.onPermissionsShown()

        verify {
            controller.handlePermissionsShown()
        }
    }

    @Test
    fun `onPermissionToggled should delegate the controller`() {
        val websitePermission = mockk<WebsitePermission>()
        val permission = slot<WebsitePermission>()

        interactor.onPermissionToggled(websitePermission)

        verify {
            controller.handlePermissionToggled(capture(permission))
        }

        assertTrue(permission.isCaptured)
        assertEquals(websitePermission, permission.captured)
    }

    @Test
    fun `onAutoplayChanged should delegate the controller`() {
        val websitePermission = mockk<AutoplayValue>()
        val permission = slot<AutoplayValue>()

        interactor.onAutoplayChanged(websitePermission)

        verify {
            controller.handleAutoplayChanged(capture(permission))
        }

        assertTrue(permission.isCaptured)
        assertEquals(websitePermission, permission.captured)
    }

    @Test
    fun `onTrackingProtectionToggled should delegate the controller`() {
        val isEnabled = true

        interactor.onTrackingProtectionToggled(isEnabled)

        verify {
            controller.handleTrackingProtectionToggled(isEnabled)
        }
    }

    @Test
    fun `onBlockedItemsClicked should delegate the controller`() {
        interactor.onBlockedItemsClicked()

        verify {
            controller.handleBlockedItemsClicked()
        }
    }
}
