/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.app.PendingIntent
import android.os.Build
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
class IntentUtilsTest {

    @Test
    @Config(sdk = [Build.VERSION_CODES.LOLLIPOP_MR1])
    fun `given a build version lower than 23, when defaultIntentPendingFlags is called, then flag 0 should be returned`() {
        Assert.assertEquals(0, IntentUtils.defaultIntentPendingFlags)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.M])
    fun `given a build version bigger than 22, when defaultIntentPendingFlags is called, then flag FLAG_IMMUTABLE should be returned`() {
        Assert.assertEquals(PendingIntent.FLAG_IMMUTABLE, IntentUtils.defaultIntentPendingFlags)
    }
}
