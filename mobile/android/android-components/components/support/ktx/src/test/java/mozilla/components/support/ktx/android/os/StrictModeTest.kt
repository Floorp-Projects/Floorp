/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import android.os.StrictMode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StrictModeTest {
    @Test
    fun `strict mode policy should be restored`() {
        StrictMode.setThreadPolicy(StrictMode.ThreadPolicy.Builder()
            .detectDiskReads()
            .penaltyLog()
            .build())
        val policy = StrictMode.getThreadPolicy()
        assertEquals(27, StrictMode.allowThreadDiskReads().resetAfter {
            // Comparing via toString because StrictMode.ThreadPolicy does not redefine equals() and each time
            // setThreadPolicy is called a new ThreadPolicy object is created (although the mask is the same)
            assertNotEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
            27
        })
        assertEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
    }
}
