/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.sitepermissions

import androidx.preference.Preference
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.focus.R
import org.mozilla.focus.settings.permissions.SitePermissionsFragment
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorage
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SitePermissionsFragmentTest {

    private val storage: SitePermissionOptionsStorage = mock()
    private lateinit var fragment: SitePermissionsFragment

    @Before
    fun setup() {
        fragment = spy(SitePermissionsFragment())
        doReturn(testContext).`when`(fragment).context
        fragment.storage = storage
    }

    @Test
    fun `GIVEN site permission fragment WHEN fragment is created THEN summary for Preference is set from storage`() {
        val cameraPreference = Preference(testContext)
        doReturn(cameraPreference).`when`(fragment).getPreference(R.string.pref_key_phone_feature_camera)
        val expectedSummary = testContext.getString(R.string.phone_feature_blocked_by_android)
        doReturn(expectedSummary).`when`(storage).getSitePermissionOptionSelectedLabel(SitePermission.CAMERA)

        fragment.initPhoneFeature(SitePermission.CAMERA)

        assertEquals(expectedSummary, cameraPreference.summary)
    }

    @Test
    fun `GIVEN site permission fragment WHEN preference is clicked THEN navigate to site permission options screen it called`() {
        val cameraPreference = Preference(testContext)
        doReturn(cameraPreference).`when`(fragment).getPreference(R.string.pref_key_phone_feature_camera)

        fragment.initPhoneFeature(SitePermission.CAMERA)
        cameraPreference.performClick()

        verify(fragment).navigateToSitePermissionOptionsScreen(SitePermission.CAMERA)
    }
}
