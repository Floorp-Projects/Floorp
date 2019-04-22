/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import android.content.Context
import android.util.AttributeSet
import org.json.JSONObject
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.lang.UnsupportedOperationException

class EngineTest {

    @Test
    fun `invokes error callback if webextensions not supported`() {
        val engine = object : Engine {
            override fun createView(context: Context, attrs: AttributeSet?): EngineView {
                throw NotImplementedError("Not needed for test")
            }

            override fun createSession(private: Boolean): EngineSession {
                throw NotImplementedError("Not needed for test")
            }

            override fun createSessionState(json: JSONObject): EngineSessionState {
                throw NotImplementedError("Not needed for test")
            }

            override fun name(): String {
                throw NotImplementedError("Not needed for test")
            }

            override fun speculativeConnect(url: String) {
                throw NotImplementedError("Not needed for test")
            }

            override val settings: Settings
                get() = throw NotImplementedError("Not needed for test")
        }

        var exception: Throwable? = null
        engine.installWebExtension("my-ext", "resource://path", onError = { _, e -> exception = e })
        assertNotNull(exception)
        assertTrue(exception is UnsupportedOperationException)
    }
}