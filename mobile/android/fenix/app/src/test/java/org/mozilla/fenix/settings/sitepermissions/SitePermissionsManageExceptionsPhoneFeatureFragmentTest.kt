/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.view.LayoutInflater
import android.view.View
import android.widget.RadioButton
import androidx.core.view.isVisible
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import io.mockk.MockKAnnotations
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
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.settings.quicksettings.AutoplayValue
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SitePermissionsManageExceptionsPhoneFeatureFragmentTest {
    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @MockK(relaxed = true)
    private lateinit var permissions: SitePermissions

    private lateinit var fragment: SitePermissionsManageExceptionsPhoneFeatureFragment

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        fragment = spyk(SitePermissionsManageExceptionsPhoneFeatureFragment())
        fragment.rootView = mockk(relaxed = true)

        every { fragment.requireContext() } returns testContext
        every { fragment.getSettings() } returns settings
    }

    @Test
    fun `GIVEN an AUTOPLAY permission WHEN onCreateView is called THEN initAutoplay is called`() {
        every { fragment.getFeature() } returns PhoneFeature.AUTOPLAY
        every { fragment.initAutoplay(permissions) } returns Unit
        every { fragment.getSitePermission() } returns permissions

        fragment.onCreateView(LayoutInflater.from(testContext), null, null)

        verify {
            fragment.initAutoplay(permissions)
            fragment.bindBlockedByAndroidContainer()
            fragment.initClearPermissionsButton()
        }
    }

    @Test
    fun `GIVEN a none AUTOPLAY permission WHEN onCreateView is called THEN initNormalFeature is called`() {
        val features = PhoneFeature.values().filter { it != PhoneFeature.AUTOPLAY }

        features.forEach {
            every { fragment.getFeature() } returns it
            every { fragment.initNormalFeature() } returns Unit
            every { fragment.getSitePermission() } returns permissions

            fragment.onCreateView(LayoutInflater.from(testContext), null, null)

            verify {
                fragment.initNormalFeature()
                fragment.bindBlockedByAndroidContainer()
                fragment.initClearPermissionsButton()
            }
        }
    }

    @Test
    fun `WHEN initAutoplay is called THEN AllowAll, BlockAll and BlockAudible radio options will be configure`() {
        every { fragment.initAutoplayOption(any(), any()) } returns Unit
        every { fragment.getSitePermission() } returns permissions
        every { settings.getSitePermissionsCustomSettingsRules() } returns getRules()

        fragment.initAutoplay()

        verify {
            fragment.initAutoplayOption(R.id.ask_to_allow_radio, any<AutoplayValue.AllowAll>())
            fragment.initAutoplayOption(R.id.block_radio, any<AutoplayValue.BlockAll>())
            fragment.initAutoplayOption(R.id.optional_radio, any<AutoplayValue.BlockAudible>())
        }
    }

    @Test
    fun `WHEN initAutoplayOption is called THEN the radio button will visible and a click listener will be attached`() {
        val radioButton = spyk(RadioButton(testContext))
        val rootView = mockk<View>()
        val autoplayValue = mockk<AutoplayValue>(relaxed = true)

        radioButton.isVisible = false

        fragment.rootView = rootView
        every { rootView.findViewById<View>(any()) } returns radioButton
        every { autoplayValue.label } returns "label"
        with(fragment) {
            every { updatedSitePermissions(any()) } returns Unit
            every { any<RadioButton>().restoreState(any()) } returns Unit
        }

        fragment.initAutoplayOption(R.id.ask_to_allow_radio, autoplayValue)

        assertTrue(radioButton.isVisible)
        assertEquals(autoplayValue.label, radioButton.text)

        with(fragment) {
            verify {
                any<RadioButton>().restoreState(autoplayValue)
            }
        }
    }

    @Test
    fun `GIVEN a AllowAll value with autoplayAudible and autoplayInaudible rules are ALLOWED WHEN isSelected is called THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.ALLOWED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())
    }

    @Test
    fun `GIVEN a AllowAll value with autoplayAudible ALLOWED and autoplayInaudible BLOCKED rules WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED
        )

        val value = AutoplayValue.AllowAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `GIVEN a AllowAll value with sitePermission autoplayAudible and autoplayInaudible are ALLOWED WHEN isSelected is called THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED
        )

        val value = AutoplayValue.AllowAll(
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
    }

    @Test
    fun `GIVEN a AllowAll value with sitePermission autoplayAudible and autoplayInaudible are BLOCKED WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.ALLOWED,
            autoplayAudible = AutoplayAction.BLOCKED
        )

        val value = AutoplayValue.AllowAll(
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
    fun `GIVEN a BlockAll value with autoplayAudible and autoplayInaudible rules are BLOCKED WHEN isSelected is called THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.BLOCKED
        )

        val value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())
    }

    @Test
    fun `GIVEN a BlockAll value with autoplayInaudible BLOCKED and autoplayAudible ALLOWED rules WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAll(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `GIVEN a BlockAll value with sitePermission autoplayAudible and autoplayInaudible are BLOCKED WHEN isSelected THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAll(
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
    }

    @Test
    fun `GIVEN a BlockAll value with sitePermission autoplayAudible ALLOWED and autoplayInaudible BLOCKED WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAll(
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
    fun `GIVEN a BlockAudible value with autoplayAudible BLOCKED and autoplayInaudible ALLOWED rules WHEN isSelected is called THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayAudible = AutoplayAction.BLOCKED,
            autoplayInaudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertTrue(value.isSelected())
    }

    @Test
    fun `GIVEN a BlockAudible value with autoplayInaudible and autoplayAudible BLOCKED rules WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.BLOCKED
        )

        val value = AutoplayValue.BlockAudible(
            label = "label",
            rules = rules,
            sitePermission = null
        )

        assertFalse(value.isSelected())
    }

    @Test
    fun `GIVEN a BlockAudible with sitePermission autoplayAudible BLOCKED and autoplayInaudible ALLOWED WHEN isSelected is called THEN isSelected will be true`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAudible(
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
    }

    @Test
    fun `GIVEN a BlockAudible with sitePermission autoplayAudible ALLOWED and autoplayInaudible BLOCKED WHEN isSelected is called THEN isSelected will be false`() {
        val rules = getRules().copy(
            autoplayInaudible = AutoplayAction.BLOCKED,
            autoplayAudible = AutoplayAction.ALLOWED
        )

        val value = AutoplayValue.BlockAudible(
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
    fun `GIVEN a AllowAll WHEN createSitePermissionsFromCustomRules is called THEN rules will included autoplayAudible and autoplayInaudible ALLOWED`() {
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
    fun `GIVEN a BlockAll WHEN createSitePermissionsFromCustomRules is called THEN rules will included autoplayAudible and autoplayInaudible BLOCKED`() {
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
    fun `GIVEN a BlockAudible WHEN createSitePermissionsFromCustomRules is called THEN rules will included autoplayAudible BLOCKED and autoplayInaudible ALLOWED`() {
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
    fun `GIVEN a AllowAll WHEN updateSitePermissions is called THEN site permissions will include autoplayAudible and autoplayInaudible ALLOWED`() {
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
    fun `GIVEN a BlockAll WHEN updateSitePermissions is called THEN site permissions will include autoplayAudible and autoplayInaudible BLOCKED`() {
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
    fun `GIVEN a BlockAudible WHEN updateSitePermissions is called THEN site permissions will include autoplayAudible BLOCKED and autoplayInaudible ALLOWED`() {
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
    fun `WHEN calling AutoplayValue values THEN values for AllowAll,BlockAll and BlockAudible will be returned`() {
        val values = AutoplayValue.values(testContext, settings, null)

        assertTrue(values.any { it is AutoplayValue.AllowAll })
        assertTrue(values.any { it is AutoplayValue.BlockAll })
        assertTrue(values.any { it is AutoplayValue.BlockAudible })
        assertEquals(3, values.size)
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
