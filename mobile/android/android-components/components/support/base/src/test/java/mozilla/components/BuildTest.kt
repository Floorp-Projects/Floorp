/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class BuildTest {
    @Test
    fun `Sanity check Build class`() {
        assertNotNull(Build.version)
        assertNotNull(Build.applicationServicesVersion)
        assertNotNull(Build.gitHash)

        assertTrue(Build.version.isNotBlank())
        assertTrue(Build.applicationServicesVersion.isNotBlank())
        assertTrue(Build.gitHash.isNotBlank())
    }
}
