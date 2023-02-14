/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import android.app.Activity
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doReturn
import java.lang.ref.WeakReference

class GeckoViewActivityContextDelegateTest {

    @Test
    fun `getActivityContext returns the same activity as set on the delegate`() {
        val mockActivity = mock<Activity>()
        val activityContextDelegate = GeckoViewActivityContextDelegate(WeakReference(mockActivity))
        assertTrue(mockActivity == activityContextDelegate.activityContext)
    }

    @Test
    fun `getActivityContext returns null when the activity is destroyed`() {
        val mockActivity = mock<Activity>()
        val activityContextDelegate = GeckoViewActivityContextDelegate(WeakReference(mockActivity))
        doReturn(true).`when`(mockActivity).isDestroyed
        assertNull(activityContextDelegate.activityContext)
    }

    @Test
    fun `getActivityContext returns null when the activity reference is null`() {
        val activityContextDelegate = GeckoViewActivityContextDelegate(WeakReference(null))
        assertNull(activityContextDelegate.activityContext)
    }
}
