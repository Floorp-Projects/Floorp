/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.AutoplayStatus
import mozilla.components.feature.sitepermissions.SitePermissionsRules
import mozilla.components.feature.sitepermissions.SitePermissionsRules.AutoplayAction
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class AutoplayValueTest {
    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `AllowAll - isSelected`() {
        var rules = getRules().copy(
            autoplayAudible = AutoplayAction.ALLOWED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        var value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())

        rules = rules.copy(
            autoplayAudible = AutoplayAction.BLOCKED
        )

        value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())

        value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.ALLOWED,
                autoplayInaudible = AutoplayStatus.ALLOWED
            )
        )

        assertTrue(value.isSelected())

        value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.BLOCKED,
                autoplayInaudible = AutoplayStatus.BLOCKED
            )
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `BlockAll - isSelected`() {
        var rules = getRules().copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.BLOCKED
        )

        var value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())

        rules = rules.copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())

        value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.BLOCKED,
                autoplayInaudible = AutoplayStatus.BLOCKED
            )
        )

        assertTrue(value.isSelected())

        value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.ALLOWED,
                autoplayInaudible = AutoplayStatus.BLOCKED
            )
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `BlockAudible - isSelected`() {
        var rules = getRules().copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        var value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())

        rules = rules.copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.BLOCKED
        )

        value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())

        value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.BLOCKED,
                autoplayInaudible = AutoplayStatus.ALLOWED
            )
        )

        assertTrue(value.isSelected())

        value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = SitePermissions(
                origin = "",
                savedAt = 0L,
                autoplayAudible = AutoplayStatus.ALLOWED,
                autoplayInaudible = AutoplayStatus.ALLOWED
            )
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `AllowAll - createSitePermissionsFromCustomRules`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.BLOCKED
        )

        every { settings.getSitePermissionsCustomSettingsRules() } returns rules

        val value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        val result = value.createSitePermissionsFromCustomRules("mozilla.org", settings)

        assertEquals(AutoplayStatus.ALLOWED, result.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, result.autoplayInaudible)
        assertEquals(rules.camera.toStatus(), result.camera)
        assertEquals(rules.location.toStatus(), result.location)
        assertEquals(rules.microphone.toStatus(), result.microphone)
        assertEquals(rules.notification.toStatus(), result.notification)
        assertEquals(rules.persistentStorage.toStatus(), result.localStorage)
        assertEquals(rules.mediaKeySystemAccess.toStatus(), result.mediaKeySystemAccess)
    }

    @Test
    fun `BlockAll - createSitePermissionsFromCustomRules`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.ALLOWED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        every { settings.getSitePermissionsCustomSettingsRules() } returns rules

        val value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        val result = value.createSitePermissionsFromCustomRules("mozilla.org", settings)

        assertEquals(AutoplayStatus.BLOCKED, result.autoplayAudible)
        assertEquals(AutoplayStatus.BLOCKED, result.autoplayInaudible)
        assertEquals(rules.camera.toStatus(), result.camera)
        assertEquals(rules.location.toStatus(), result.location)
        assertEquals(rules.microphone.toStatus(), result.microphone)
        assertEquals(rules.notification.toStatus(), result.notification)
        assertEquals(rules.persistentStorage.toStatus(), result.localStorage)
        assertEquals(rules.mediaKeySystemAccess.toStatus(), result.mediaKeySystemAccess)
    }

    @Test
    fun `BlockAudible - createSitePermissionsFromCustomRules`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.ALLOWED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        every { settings.getSitePermissionsCustomSettingsRules() } returns rules

        val value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        val result = value.createSitePermissionsFromCustomRules("mozilla.org", settings)

        assertEquals(AutoplayStatus.BLOCKED, result.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, result.autoplayInaudible)
        assertEquals(rules.camera.toStatus(), result.camera)
        assertEquals(rules.location.toStatus(), result.location)
        assertEquals(rules.microphone.toStatus(), result.microphone)
        assertEquals(rules.notification.toStatus(), result.notification)
        assertEquals(rules.persistentStorage.toStatus(), result.localStorage)
        assertEquals(rules.mediaKeySystemAccess.toStatus(), result.mediaKeySystemAccess)
    }

    @Test
    fun `AllowAll - updateSitePermissions`() {
        val sitePermissions = SitePermissions(
            origin = "origin",
            savedAt = 0L,
            autoplayAudible = AutoplayStatus.BLOCKED,
            autoplayInaudible = AutoplayStatus.BLOCKED
        )

        val value = AutoplayValue.AllowAll(
            label = "label",
            rules = mock(),
            sitePermission = null
        )

        val result = value.updateSitePermissions(sitePermissions)

        assertEquals(AutoplayStatus.ALLOWED, result.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, result.autoplayInaudible)
        assertEquals(sitePermissions.camera, result.camera)
        assertEquals(sitePermissions.location, result.location)
        assertEquals(sitePermissions.microphone, result.microphone)
        assertEquals(sitePermissions.notification, result.notification)
        assertEquals(sitePermissions.localStorage, result.localStorage)
        assertEquals(sitePermissions.mediaKeySystemAccess, result.mediaKeySystemAccess)
    }

    @Test
    fun `BlockAll - updateSitePermissions`() {
        val sitePermissions = SitePermissions(
            origin = "origin",
            savedAt = 0L,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.ALLOWED
        )

        val value = AutoplayValue.BlockAll(
            label = "label",
            rules = mock(),
            sitePermission = null
        )

        val result = value.updateSitePermissions(sitePermissions)

        assertEquals(AutoplayStatus.BLOCKED, result.autoplayAudible)
        assertEquals(AutoplayStatus.BLOCKED, result.autoplayInaudible)
        assertEquals(sitePermissions.camera, result.camera)
        assertEquals(sitePermissions.location, result.location)
        assertEquals(sitePermissions.microphone, result.microphone)
        assertEquals(sitePermissions.notification, result.notification)
        assertEquals(sitePermissions.localStorage, result.localStorage)
        assertEquals(sitePermissions.mediaKeySystemAccess, result.mediaKeySystemAccess)
    }

    @Test
    fun `BlockAudible - updateSitePermissions`() {
        val sitePermissions = SitePermissions(
            origin = "origin",
            savedAt = 0L,
            autoplayAudible = AutoplayStatus.ALLOWED,
            autoplayInaudible = AutoplayStatus.BLOCKED
        )

        val value = AutoplayValue.BlockAudible(
            label = "label",
            rules = mock(),
            sitePermission = null
        )

        val result = value.updateSitePermissions(sitePermissions)

        assertEquals(AutoplayStatus.BLOCKED, result.autoplayAudible)
        assertEquals(AutoplayStatus.ALLOWED, result.autoplayInaudible)
        assertEquals(sitePermissions.camera, result.camera)
        assertEquals(sitePermissions.location, result.location)
        assertEquals(sitePermissions.microphone, result.microphone)
        assertEquals(sitePermissions.notification, result.notification)
        assertEquals(sitePermissions.localStorage, result.localStorage)
        assertEquals(sitePermissions.mediaKeySystemAccess, result.mediaKeySystemAccess)
    }

    @Test
    fun `values - contains the right values`() {
        val values = AutoplayValue.values(testContext, settings, null)

        assertTrue(values.any { it is AutoplayValue.AllowAll })
        assertTrue(values.any { it is AutoplayValue.BlockAll })
        assertTrue(values.any { it is AutoplayValue.BlockAudible })
    }

    private fun getRules() = SitePermissionsRules(
        camera = Action.ASK_TO_ALLOW,
        location = Action.ASK_TO_ALLOW,
        microphone = Action.ASK_TO_ALLOW,
        notification = Action.ASK_TO_ALLOW,
        autoplayAudible = AutoplayAction.BLOCKED,
        autoplayInaudible = AutoplayAction.BLOCKED,
        persistentStorage = Action.ASK_TO_ALLOW,
        mediaKeySystemAccess = Action.ASK_TO_ALLOW
    )
}
