package org.mozilla.fenix.whatsnew

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(AndroidJUnit4::class)
@Config(application = TestApplication::class)
class WhatsNewVersionTest {
    @Test
    fun testMajorVersionNumber() {
        val versionOne = WhatsNewVersion("1.2.0")
        assertEquals(1, versionOne.majorVersionNumber)

        val versionTwo = WhatsNewVersion("2.4.0")
        assertNotEquals(1, versionTwo.majorVersionNumber)
    }
}
