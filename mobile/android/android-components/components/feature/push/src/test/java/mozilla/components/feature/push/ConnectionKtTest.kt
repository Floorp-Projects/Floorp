/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push

import mozilla.appservices.push.BridgeType
import org.junit.Assert.assertEquals
import org.junit.Test

class ConnectionKtTest {
    @Test
    fun `ServiceType to BridgeType`() {
        assertEquals(BridgeType.FCM, ServiceType.FCM.toBridgeType())
        assertEquals(BridgeType.ADM, ServiceType.ADM.toBridgeType())
    }

    @Test
    fun `Protocol to string`() {
        assertEquals("http", Protocol.HTTP.asString())
        assertEquals("https", Protocol.HTTPS.asString())
    }
}
