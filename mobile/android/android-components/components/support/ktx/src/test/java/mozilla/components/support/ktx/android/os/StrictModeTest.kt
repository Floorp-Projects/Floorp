/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import android.os.StrictMode
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class StrictModeTest {

    @Test
    fun `strict mode policy should be restored`() {
        StrictMode.setThreadPolicy(
            StrictMode.ThreadPolicy.Builder()
                .detectDiskReads()
                .penaltyLog()
                .build(),
        )
        val policy = StrictMode.getThreadPolicy()
        assertEquals(
            27,
            StrictMode.allowThreadDiskReads().resetAfter {
                // Comparing via toString because StrictMode.ThreadPolicy does not redefine equals() and each time
                // setThreadPolicy is called a new ThreadPolicy object is created (although the mask is the same)
                assertNotEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
                27
            },
        )
        assertEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
    }

    @Test
    fun `strict mode policy should be restored if function block throws an error`() {
        val policy = StrictMode.ThreadPolicy.Builder()
            .detectDiskReads()
            .penaltyLog()
            .build().apply {
                StrictMode.setThreadPolicy(this)
            }

        val exceptionCaught: Boolean

        assertEquals(policy.toString(), StrictMode.getThreadPolicy().toString())

        try {
            StrictMode.allowThreadDiskReads().resetAfter {
                assertNotEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
                throw RuntimeException("Boing!")
            }
        } catch (e: RuntimeException) {
            exceptionCaught = true
        }

        assertTrue(exceptionCaught)
        assertEquals(policy.toString(), StrictMode.getThreadPolicy().toString())
    }
}
