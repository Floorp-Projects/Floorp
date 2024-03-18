/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.activity

import android.content.pm.ActivityInfo
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.engine.activity.OrientationDelegate
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.geckoview.AllowOrDeny

@RunWith(AndroidJUnit4::class)
class GeckoScreenOrientationDelegateTest {
    @Test
    fun `GIVEN a delegate is set WHEN the orientation should be locked THEN call this on the delegate`() {
        val activityDelegate = mock<OrientationDelegate>()
        val geckoDelegate = GeckoScreenOrientationDelegate(activityDelegate)

        geckoDelegate.onOrientationLock(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)

        verify(activityDelegate).onOrientationLock(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE)
    }

    @Test
    fun `GIVEN a delegate is set WHEN the orientation should be locked THEN return ALLOW depending on the delegate response`() {
        val activityDelegate = object : OrientationDelegate {
            override fun onOrientationLock(requestedOrientation: Int) = true
        }
        val geckoDelegate = GeckoScreenOrientationDelegate(activityDelegate)

        val result = geckoDelegate.onOrientationLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT)

        assertTrue(result.poll(1) == AllowOrDeny.ALLOW)
    }

    @Test
    fun `GIVEN a delegate is set WHEN the orientation should be locked THEN return DENY depending on the delegate response`() {
        val activityDelegate = object : OrientationDelegate {
            override fun onOrientationLock(requestedOrientation: Int) = false
        }
        val geckoDelegate = GeckoScreenOrientationDelegate(activityDelegate)

        val result = geckoDelegate.onOrientationLock(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT)

        assertTrue(result.poll(1) == AllowOrDeny.DENY)
    }

    @Test
    fun `GIVEN a delegate is set WHEN the orientation should be unlocked THEN call this on the delegate`() {
        val activityDelegate = mock<OrientationDelegate>()
        val geckoDelegate = GeckoScreenOrientationDelegate(activityDelegate)

        geckoDelegate.onOrientationUnlock()

        verify(activityDelegate).onOrientationUnlock()
    }
}
