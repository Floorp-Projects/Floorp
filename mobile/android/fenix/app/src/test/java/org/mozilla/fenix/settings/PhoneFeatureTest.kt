/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.Manifest
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.concept.engine.permission.SitePermissions.Status
import mozilla.components.feature.sitepermissions.SitePermissionsRules.Action
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class PhoneFeatureTest {

    @MockK private lateinit var sitePermissions: SitePermissions
    @MockK private lateinit var settings: Settings

    @Before
    fun setup() {
        MockKAnnotations.init(this)
    }

    @Test
    fun `getStatus throws if both values are null`() {
        var exception: IllegalArgumentException? = null
        try {
            PhoneFeature.AUTOPLAY_AUDIBLE.getStatus()
        } catch (e: java.lang.IllegalArgumentException) {
            exception = e
        }
        assertNotNull(exception)
    }

    @Test
    fun `getStatus returns value from site permissions`() {
        every { sitePermissions.notification } returns Status.BLOCKED
        assertEquals(Status.BLOCKED, PhoneFeature.NOTIFICATION.getStatus(sitePermissions, settings))
    }

    @Test
    fun `getStatus returns value from settings`() {
        every {
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY_INAUDIBLE, Action.ALLOWED)
        } returns Action.ALLOWED
        assertEquals(Status.ALLOWED, PhoneFeature.AUTOPLAY_INAUDIBLE.getStatus(settings = settings))
    }

    @Test
    fun getLabel() {
        assertEquals("Camera", PhoneFeature.CAMERA.getLabel(testContext))
        assertEquals("Location", PhoneFeature.LOCATION.getLabel(testContext))
        assertEquals("Microphone", PhoneFeature.MICROPHONE.getLabel(testContext))
        assertEquals("Notification", PhoneFeature.NOTIFICATION.getLabel(testContext))
        assertEquals("Autoplay", PhoneFeature.AUTOPLAY_AUDIBLE.getLabel(testContext))
        assertEquals("Autoplay", PhoneFeature.AUTOPLAY_INAUDIBLE.getLabel(testContext))
        assertEquals("Autoplay", PhoneFeature.AUTOPLAY.getLabel(testContext))
    }

    @Test
    fun getPreferenceId() {
        assertEquals(R.string.pref_key_phone_feature_camera, PhoneFeature.CAMERA.getPreferenceId())
        assertEquals(R.string.pref_key_phone_feature_location, PhoneFeature.LOCATION.getPreferenceId())
        assertEquals(R.string.pref_key_phone_feature_microphone, PhoneFeature.MICROPHONE.getPreferenceId())
        assertEquals(R.string.pref_key_phone_feature_notification, PhoneFeature.NOTIFICATION.getPreferenceId())
        assertEquals(R.string.pref_key_browser_feature_autoplay_audible_v2, PhoneFeature.AUTOPLAY_AUDIBLE.getPreferenceId())
        assertEquals(R.string.pref_key_browser_feature_autoplay_inaudible_v2, PhoneFeature.AUTOPLAY_INAUDIBLE.getPreferenceId())
        assertEquals(R.string.pref_key_browser_feature_autoplay_v2, PhoneFeature.AUTOPLAY.getPreferenceId())
    }

    @Test
    fun `getAction returns value from settings`() {
        every {
            settings.getSitePermissionsPhoneFeatureAction(PhoneFeature.AUTOPLAY_AUDIBLE, Action.BLOCKED)
        } returns Action.ASK_TO_ALLOW
        assertEquals(Action.ASK_TO_ALLOW, PhoneFeature.AUTOPLAY_AUDIBLE.getAction(settings))
    }

    @Test
    fun findFeatureBy() {
        assertEquals(PhoneFeature.CAMERA, PhoneFeature.findFeatureBy(arrayOf(Manifest.permission.CAMERA)))
        assertEquals(PhoneFeature.MICROPHONE, PhoneFeature.findFeatureBy(arrayOf(Manifest.permission.RECORD_AUDIO)))
    }
}
