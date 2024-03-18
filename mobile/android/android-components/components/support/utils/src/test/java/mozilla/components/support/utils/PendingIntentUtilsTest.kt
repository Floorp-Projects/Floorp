/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.app.PendingIntent
import android.os.Build
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class PendingIntentUtilsTest {
    @Test
    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP_MR1]) // highest API level for which to return 0
    fun `GIVEN an Android L device WHEN defaultFlags is called THEN return 0`() {
        assertEquals(0, PendingIntentUtils.defaultFlags)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M]) // first API level for which to return FLAG_IMMUTABLE
    fun `GIVEN an Android M device WHEN defaultFlags is called THEN return FLAG_IMMUTABLE`() {
        assertEquals(PendingIntent.FLAG_IMMUTABLE, PendingIntentUtils.defaultFlags)
    }
}
