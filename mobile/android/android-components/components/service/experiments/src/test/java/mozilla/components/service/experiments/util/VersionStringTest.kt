/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.experiments.util

import org.junit.Assert
import org.junit.Test

class VersionStringTest {

    @Test
    fun compareTo() {
        // Comparing two exact versions returns 0
        Assert.assertEquals(0, VersionString("1.0.0").compareTo(VersionString("1.0.0")))

        // Comparing with left padded zeroes returns 0
        Assert.assertEquals(0, VersionString("1.0.0").compareTo(VersionString("1.00.00")))
        Assert.assertEquals(0, VersionString("1.1.1").compareTo(VersionString("1.01.0001")))

        // Comparing to extra dotted sections with zero returns 0
        Assert.assertEquals(0, VersionString("1").compareTo(VersionString("1.0.000")))
        Assert.assertEquals(0, VersionString("1.1").compareTo(VersionString("1.1.000.0")))

        // Comparing a higher version to a lower version returns 1
        Assert.assertEquals(1, VersionString("1.1.0").compareTo(VersionString("1.0.0")))
        Assert.assertEquals(1, VersionString("1.1.54321").compareTo(VersionString("1.1.12345")))
        Assert.assertEquals(1, VersionString("51.1.0").compareTo(VersionString("50.1.0")))
        Assert.assertEquals(1, VersionString("1.1.0").compareTo(VersionString("1.0.1")))

        // Comparing a lower version to a higher version returns -1
        Assert.assertEquals(-1, VersionString("1.0.0").compareTo(VersionString("1.1.0")))
        Assert.assertEquals(-1, VersionString("1.1.12345").compareTo(VersionString("1.1.54321")))
        Assert.assertEquals(-1, VersionString("50.1.0").compareTo(VersionString("51.1.0")))
        Assert.assertEquals(-1, VersionString("1.0.1").compareTo(VersionString("1.1.0")))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `invalid VersionString's throw IllegalArgumentException`() {
        VersionString("Invalid").compareTo(VersionString("Also_invalid"))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `invalid left VersionString makes it throw IllegalArgumentException`() {
        VersionString("Invalid").compareTo(VersionString("1.0.0"))
    }

    @Test(expected = IllegalArgumentException::class)
    fun `invalid right VersionString makes it throw IllegalArgumentException`() {
        VersionString("1.0.0").compareTo(VersionString("Invalid"))
    }

    @Test
    fun equals() {
        Assert.assertEquals(VersionString("1.0.0"), VersionString("1.0.0"))
        Assert.assertNotEquals(VersionString("1.0.0"), VersionString("1.0.1"))
    }

    @Test
    fun isValid() {
        Assert.assertTrue(VersionString("1.0.0").isValid())
        Assert.assertTrue(VersionString("1.0.1").isValid())
        Assert.assertTrue(VersionString("1.0").isValid())
        Assert.assertTrue(VersionString("10.4.123545").isValid())

        Assert.assertFalse(VersionString("Invalid").isValid())
        Assert.assertFalse(VersionString("Nightly 191103 18:03").isValid())
        Assert.assertFalse(VersionString("a.b.c").isValid())
        Assert.assertFalse(VersionString("Not.a.version").isValid())
        Assert.assertFalse(VersionString("12.1.1A").isValid())
        Assert.assertFalse(VersionString("").isValid())
    }
}
