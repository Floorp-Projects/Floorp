/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.content.Intent
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doThrow
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SafeIntentTest {
    @Test
    fun `getStringArrayListExtra returns null if intent throws OutOfMemoryError`() {
        val intent = mock(Intent::class.java)
        doThrow(OutOfMemoryError::class.java)
                .`when`(intent).getStringArrayListExtra(ArgumentMatchers.anyString())

        assertNull(SafeIntent(intent).getStringArrayListExtra("mozilla"))
    }
}
