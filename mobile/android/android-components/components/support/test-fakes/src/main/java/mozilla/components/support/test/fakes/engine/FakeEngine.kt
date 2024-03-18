/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes.engine

import android.content.Context
import android.util.AttributeSet
import android.util.JsonReader
import mozilla.components.concept.base.profiler.Profiler
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject
import org.junit.Assert.assertEquals

/**
 * A fake [Engine] implementation to be used in tests that require an [Engine] instance.
 *
 * @param expectToRestoreRealEngineSessionState Whether this fake engine should expect to restore
 * engine sessions from actual engine session state JSON (e.g. from GeckoView). Otherwise this fake
 * will expect to always deal with [FakeEngineSessionState] instances.
 */
class FakeEngine(
    private val expectToRestoreRealEngineSessionState: Boolean = false,
) : Engine {
    override val version: EngineVersion
        get() = throw NotImplementedError()

    override fun createView(context: Context, attrs: AttributeSet?): EngineView =
        FakeEngineView(context)

    override fun createSession(private: Boolean, contextId: String?): EngineSession =
        throw UnsupportedOperationException()

    override fun createSessionState(json: JSONObject) = FakeEngineSessionState(json.getString("engine"))

    override fun createSessionStateFrom(reader: JsonReader): EngineSessionState {
        if (expectToRestoreRealEngineSessionState) {
            reader.skipValue()
            return FakeEngineSessionState("<real state>")
        }

        var value: String? = null

        reader.beginObject()

        if (reader.hasNext()) {
            assertEquals("engine", reader.nextName())
            value = reader.nextString()
        }

        reader.endObject()

        return FakeEngineSessionState(value ?: "---")
    }

    override fun name(): String = "fake_engine"

    override fun speculativeConnect(url: String) =
        throw UnsupportedOperationException()

    override val profiler: Profiler
        get() = throw NotImplementedError()

    override val settings: Settings = DefaultSettings()
}
