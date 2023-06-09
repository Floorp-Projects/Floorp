/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.content.Context
import androidx.preference.Preference
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.concept.engine.permission.SitePermissions
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.PhoneFeature
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class SitePermissionsDetailsExceptionsFragmentTest {
    @MockK(relaxed = true)
    private lateinit var settings: Settings

    @MockK(relaxed = true)
    private lateinit var permissions: SitePermissions

    private lateinit var fragment: SitePermissionsDetailsExceptionsFragment
    private lateinit var context: Context

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        context = spyk(testContext)
        fragment = spyk(SitePermissionsDetailsExceptionsFragment())

        fragment.sitePermissions = permissions

        every { permissions.origin } returns "mozilla.org"
        every { fragment.provideContext() } returns context
        every { fragment.provideSettings() } returns settings
    }

    @Test
    fun `WHEN bindCategoryPhoneFeatures is called THEN all categories must be initialized`() {
        every { fragment.initPhoneFeature(any()) } returns Unit
        every { fragment.initAutoplayFeature() } returns Unit
        every { fragment.bindClearPermissionsButton() } returns Unit

        fragment.bindCategoryPhoneFeatures()

        verify {
            fragment.initPhoneFeature(PhoneFeature.CAMERA)
            fragment.initPhoneFeature(PhoneFeature.LOCATION)
            fragment.initPhoneFeature(PhoneFeature.MICROPHONE)
            fragment.initPhoneFeature(PhoneFeature.NOTIFICATION)
            fragment.initPhoneFeature(PhoneFeature.PERSISTENT_STORAGE)
            fragment.initPhoneFeature(PhoneFeature.MEDIA_KEY_SYSTEM_ACCESS)
            fragment.initAutoplayFeature()
            fragment.bindClearPermissionsButton()
        }
    }

    @Test
    fun `WHEN initPhoneFeature is called THEN the feature label must be bind and a click listener must be attached`() {
        val feature = PhoneFeature.CAMERA
        val label = "label"
        val preference = Preference(context)

        every { context.getString(R.string.phone_feature_blocked_by_android) } returns label
        every { fragment.getPreference((any())) } returns preference
        every { fragment.navigateToPhoneFeature((any())) } returns Unit

        fragment.initPhoneFeature(feature)

        assertEquals(label, preference.summary)

        preference.performClick()

        verify {
            fragment.navigateToPhoneFeature(feature)
        }
    }

    @Test
    fun `WHEN initAutoplayFeature THEN the autoplay label must be bind and a click listener must be attached`() {
        val label = "label"
        val preference = Preference(context)

        every { fragment.getAutoplayLabel() } returns label
        every { fragment.getPreference((any())) } returns preference
        every { fragment.navigateToPhoneFeature((any())) } returns Unit

        fragment.initAutoplayFeature()

        assertEquals(label, preference.summary)

        preference.performClick()

        verify {
            fragment.navigateToPhoneFeature(PhoneFeature.AUTOPLAY)
        }
    }
}
