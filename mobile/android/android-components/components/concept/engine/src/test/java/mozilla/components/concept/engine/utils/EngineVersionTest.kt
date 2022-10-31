/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.utils

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class EngineVersionTest {
    @Test
    fun `Parse common Gecko versions`() {
        EngineVersion.parse("67.0.1").assertIs(67, 0, 1)
        EngineVersion.parse("68.0").assertIs(68, 0, 0)
        EngineVersion.parse("69.0a1").assertIs(69, 0, 0, "a1")
        EngineVersion.parse("70.0b1").assertIs(70, 0, 0, "b1")
        EngineVersion.parse("68.3esr").assertIs(68, 3, 0, "esr")
    }

    @Test
    fun `Parse common Chrome versions`() {
        EngineVersion.parse("75.0.3770").assertIs(75, 0, 3770)
        EngineVersion.parse("76.0.3809").assertIs(76, 0, 3809)
        EngineVersion.parse("77.0").assertIs(77, 0, 0)
    }

    @Test
    fun `Parse invalid versions`() {
        assertNull(EngineVersion.parse("Hello World"))
        assertNull(EngineVersion.parse("1.a"))
    }

    @Test
    fun `Comparing versions`() {
        assertTrue("68.0".toVersion() > "67.5.9".toVersion())
        assertTrue("68.0.1".toVersion() == "68.0.1".toVersion())
        assertTrue("76.0.3809".toVersion() < "77.0".toVersion())
        assertTrue("69.0a1".toVersion() > "69.0".toVersion())
        assertTrue("67.0.1 ".toVersion() < "67.0.2".toVersion())
        assertTrue("68.3esr".toVersion() < "70.0b1".toVersion())
        assertTrue("67.0".toVersion() < "67.0a1".toVersion())
        assertTrue("67.0a1".toVersion() < "67.0b1".toVersion())
        assertEquals(0, "68.0.1".compareTo("68.0.1"))
    }

    @Test
    fun `Comparing with isAtLeast`() {
        assertTrue("68.0.0".toVersion().isAtLeast(68))
        assertTrue("68.0.0".toVersion().isAtLeast(67, 0, 7))
        assertFalse("68.0.0".toVersion().isAtLeast(69))
        assertTrue("76.0.3809".toVersion().isAtLeast(76, 0, 3809))
        assertTrue("76.0.3809".toVersion().isAtLeast(76, 0, 3808))
        assertFalse("76.0.3809".toVersion().isAtLeast(76, 0, 3810))
        assertTrue("1.2.25".toVersion().isAtLeast(1, 1, 10))
        assertTrue("1.2.25".toVersion().isAtLeast(1, 1, 25))
        assertTrue("1.2.25".toVersion().isAtLeast(1, 2, 25))
    }

    @Test
    fun `toString returns clean version string`() {
        assertEquals("1.0.0", "1.0.0".toVersion().toString())
        assertEquals("76.0.3809", "76.0.3809".toVersion().toString())
        assertEquals("67.0.0a1", "67.0a1".toVersion().toString())
        assertEquals("68.3.0esr", "68.3esr".toVersion().toString())
        assertEquals("68.0.0", "68.0".toVersion().toString())
    }
}

private fun String.toVersion() = EngineVersion.parse(this)!!

private fun EngineVersion?.assertIs(
    major: Int,
    minor: Int,
    patch: Long,
    metadata: String? = null,
) {
    assertNotNull(this!!)

    assertEquals(major, this.major)
    assertEquals(minor, this.minor)
    assertEquals(patch, this.patch)

    if (metadata == null) {
        assertNull(this.metadata)
    } else {
        assertEquals(metadata, this.metadata)
    }
}
