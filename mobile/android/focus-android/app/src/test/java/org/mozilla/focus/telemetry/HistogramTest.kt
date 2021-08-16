/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class HistogramTest {

    @Test
    @Suppress("Deprecation")
    fun testAddLoadToHistogram() {
        TelemetryWrapper.addLoadToHistogram("https://www.mozilla.org", 99L)
        TelemetryWrapper.addLoadToHistogram("https://www.mozilla.org/en-US/MPL/", 199L)

        val desiredHistogram = IntArray(200)
        desiredHistogram[0] = 1
        desiredHistogram[1] = 1
        val desiredNumUri = 2
        val desiredDomainMap = HashSet<String>()
        desiredDomainMap.add("mozilla.org")

        val actualHistogram = TelemetryWrapper.histogram
        val actualNumUri = TelemetryWrapper.numUri
        val actualDomainMap = TelemetryWrapper.domainMap

        assertTrue(actualHistogram.contentEquals(desiredHistogram))
        assertEquals(actualHistogram[0], desiredHistogram[0])
        assertEquals(actualHistogram[1], desiredHistogram[1])
        assertEquals(desiredNumUri, actualNumUri)
        assertEquals(actualDomainMap, desiredDomainMap)
        assertEquals(actualDomainMap.size, desiredDomainMap.size)
    }
}
