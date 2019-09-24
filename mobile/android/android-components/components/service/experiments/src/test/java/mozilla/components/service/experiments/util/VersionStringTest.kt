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

    @Test
    fun equals() {
        Assert.assertEquals(VersionString("1.0.0"), VersionString("1.0.0"))
        Assert.assertNotEquals(VersionString("1.0.0"), VersionString("1.0.1"))
    }
}
