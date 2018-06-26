/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mozilla.geckoview.GeckoRuntime
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class GeckoEngineTest {

    private val runtime: GeckoRuntime = mock(GeckoRuntime::class.java)

    @Test
    fun testCreateView() {
        Assert.assertTrue(GeckoEngine(runtime).createView(RuntimeEnvironment.application) is GeckoEngineView)
    }

    @Test
    fun testCreateSession() {
        Assert.assertTrue(GeckoEngine(runtime).createSession() is GeckoEngineSession)
    }

    @Test
    fun testName() {
        Assert.assertEquals("Gecko", GeckoEngine(runtime).name())
    }
}