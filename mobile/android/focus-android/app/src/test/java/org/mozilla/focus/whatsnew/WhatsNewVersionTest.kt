/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.whatsnew

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class WhatsNewVersionTest {
    @Test
    fun testMajorVersionNumber() {
        val versionOne = WhatsNewVersion("1.2.0")
        assertEquals(1, versionOne.majorVersionNumber)

        val versionTwo = WhatsNewVersion("2.4.0")
        assertNotEquals(1, versionTwo.majorVersionNumber)
    }
}
