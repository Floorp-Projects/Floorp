/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.utils

import android.app.PendingIntent
import org.junit.Assert.assertEquals
import org.junit.Test

class IntentUtilsTest {

    @Test
    fun `given a build version lower than 23, when defaultIntentPendingFlags is called, then flag 0 should be returned`() {
        assertEquals(0, IntentUtils.defaultIntentPendingFlags(22))
    }

    @Test
    fun `given a build version bigger than 22, when defaultIntentPendingFlags is called, then flag FLAG_IMMUTABLE should be returned`() {
        assertEquals(PendingIntent.FLAG_IMMUTABLE, IntentUtils.defaultIntentPendingFlags(23))
    }
}
