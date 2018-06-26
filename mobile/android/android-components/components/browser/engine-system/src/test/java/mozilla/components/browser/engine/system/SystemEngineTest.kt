/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class SystemEngineTest {

    @Test
    fun testCreateView() {
        assertTrue(SystemEngine().createView(RuntimeEnvironment.application) is SystemEngineView)
    }

    @Test
    fun testCreateSession() {
        assertTrue(SystemEngine().createSession() is SystemEngineSession)
    }

    @Test
    fun testName() {
        assertEquals("System", SystemEngine().name())
    }
}