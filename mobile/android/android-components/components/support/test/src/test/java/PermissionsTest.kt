/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.robolectric

import android.Manifest.permission.INTERNET
import junit.framework.Assert.assertFalse
import junit.framework.Assert.assertTrue
import mozilla.components.support.ktx.android.content.isPermissionGranted
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class PermissionsTest {

    @Test
    fun `after call grantPermission this permission must be granted `() {
        val context = RuntimeEnvironment.application

        var isGranted = context.isPermissionGranted(INTERNET)
        assertFalse(isGranted)

        grantPermission(INTERNET)
        isGranted = context.isPermissionGranted(INTERNET)

        assertTrue(isGranted)
    }
}
