/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verifyNoInteractions
import java.io.File

@RunWith(AndroidJUnit4::class)
class FennecProfileTest {
    @Test
    fun `default fennec profile`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "fennec_default.txt"
        )

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        Log.w("SKDBG", profile.path)
        assertTrue(profile.path.endsWith("/profiles/10aaayu4.default"))
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `mozillazine default profile`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "mozillazine_default.txt"
        )

        assertNotNull(profile!!)
        assertFalse(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/Profiles/qioxtndq.default"))
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `mozillazine multiple profiles`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "mozillazine_multiple.txt"
        )

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("alicew", profile.name)
        assertEquals("D:\\Mozilla\\Firefox\\Profiles\\alicew", profile.path)
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `desktop profiles`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "desktop.txt"
        )

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/Profiles/xvcf5yup.default"))
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `profiles-ini not existing in path`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(testContext, crashReporter, getTestPath("profiles"))
        assertNull(profile)
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `with comments`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "with_comments.txt"
        )

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/10aaayu4.default"))
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `weird broken`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "broken.txt"
        )

        assertNotNull(profile!!)
        assertEquals("Fennec", profile.name)
        assertTrue(profile.default)
        assertTrue(profile.path.endsWith("/profiles/fennec-default"))
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `multiple profiles without default`() {
        val crashReporter: CrashReporting = mock()
        val profile = FennecProfile.findDefault(
            testContext, crashReporter, getTestPath("profiles"), "no_default.txt"
        )

        assertNotNull(profile!!)
        assertEquals("default", profile.name)
        assertFalse(profile.default)
        assertTrue(profile.path.endsWith("/profiles/Profiles/default"))
        verifyNoInteractions(crashReporter)
    }
}

fun getTestPath(dir: String): File {
    return FennecProfileTest::class.java.classLoader!!
        .getResource(dir).file
        .let { File(it) }
}
