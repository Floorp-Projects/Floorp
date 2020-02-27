/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.location

import junit.framework.TestCase.assertNull
import kotlinx.coroutines.runBlocking
import org.junit.Test

class LocationServiceTest {
    @Test
    fun `dummy implementation returns null`() {
        runBlocking {
            assertNull(LocationService.dummy().fetchRegion(false))
            assertNull(LocationService.dummy().fetchRegion(true))
            assertNull(LocationService.dummy().fetchRegion(false))
        }
    }
}
