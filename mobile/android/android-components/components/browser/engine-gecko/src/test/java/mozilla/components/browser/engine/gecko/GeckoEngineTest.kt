/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko

import android.content.Context
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class GeckoEngineTest {

    private val context: Context = RuntimeEnvironment.application

    @Test
    fun testCreateView() {
        Assert.assertTrue(GeckoEngine(context).createView(RuntimeEnvironment.application) is GeckoEngineView)
    }

    @Test
    fun testCreateSession() {
        Assert.assertTrue(GeckoEngine(context).createSession() is GeckoEngineSession)
    }

    @Test
    fun testName() {
        Assert.assertEquals("Gecko", GeckoEngine(context).name())
    }
}