/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.window

import mozilla.components.browser.engine.gecko.GeckoEngineSession
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class GeckoWindowRequestTest {

    @Test
    fun testPrepare() {
        val engineSession: GeckoEngineSession = mock()
        val windowRequest = GeckoWindowRequest("mozilla.org", engineSession)
        assertEquals(engineSession, windowRequest.prepare())
    }
}
