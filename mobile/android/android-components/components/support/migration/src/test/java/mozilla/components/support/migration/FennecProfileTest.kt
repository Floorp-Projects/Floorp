/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class FennecProfileTest {
    @Test
    fun `default fennec profile`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "fennec_default.txt")

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        Log.w("SKDBG", profile.path)
        assertTrue(profile.path.endsWith("/profiles/10aaayu4.default"))
    }

    @Test
    fun `mozillazine default profile`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "mozillazine_default.txt")

        assertNotNull(profile!!)
        assertFalse(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/Profiles/qioxtndq.default"))
    }

    @Test
    fun `mozillazine multiple profiles`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "mozillazine_multiple.txt")

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("alicew", profile.name)
        assertEquals("D:\\Mozilla\\Firefox\\Profiles\\alicew", profile.path)
    }

    @Test
    fun `desktop profiles`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "desktop.txt")

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/Profiles/xvcf5yup.default"))
    }

    @Test
    fun `profiles-ini not existing in path`() {
        val profile = FennecProfile.findDefault(testContext, getTestPath())
        assertNull(profile)
    }

    @Test
    fun `with comments`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "with_comments.txt")

        assertNotNull(profile!!)
        assertTrue(profile.default)
        assertEquals("default", profile.name)
        assertTrue(profile.path.endsWith("/profiles/10aaayu4.default"))
    }

    @Test
    fun `weird broken`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "broken.txt")

        assertNotNull(profile!!)
        assertEquals("Fennec", profile.name)
        assertTrue(profile.default)
        assertTrue(profile.path.endsWith("/profiles/fennec-default"))
    }

    @Test
    fun `multiple profiles without default`() {
        val profile = FennecProfile.findDefault(
            testContext, getTestPath(), "no_default.txt")

        assertNotNull(profile!!)
        assertEquals("default", profile.name)
        assertFalse(profile.default)
        assertTrue(profile.path.endsWith("/profiles/Profiles/default"))
    }
}

private fun getTestPath(): File {
    return FennecProfileTest::class.java.classLoader!!
        .getResource("profiles").file
        .let { File(it) }
}
