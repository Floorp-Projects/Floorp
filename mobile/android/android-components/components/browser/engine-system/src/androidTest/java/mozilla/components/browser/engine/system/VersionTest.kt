/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertTrue
import org.junit.Test

class VersionTest {
    @Test
    fun testParsingOfActualWebViewVersion() {
        runBlocking(Dispatchers.Main) {
            val context: Context = ApplicationProvider.getApplicationContext()
            val engine = SystemEngine(context)
            val version = engine.version

            assertTrue(version.major > 60)

            // 60.0.3112 was released 2017-07-31.
            assertTrue(version.isAtLeast(60, 0, 3113))
        }
    }
}
