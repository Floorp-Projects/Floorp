/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus

import android.content.Context
import android.util.AttributeSet
import android.util.JsonReader
import android.util.JsonWriter
import mozilla.components.browser.engine.gecko.profiler.Profiler
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.engine.EngineView
import mozilla.components.concept.engine.Settings
import mozilla.components.concept.engine.utils.EngineVersion
import org.json.JSONObject

/**
 * [FocusApplication] override for unit tests. This allows us to override some parameters and inputs
 * since an application object gets created without much control otherwise.
 */
class TestFocusApplication : FocusApplication() {
    override val components: Components by lazy {
        Components(this, engineOverride = FakeEngine())
    }
}

// Borrowed this from AC unit tests. This is something we should consider moving to support-test, so
// that everyone who needs an Engine in unit tests can use it. It also allows us to enhance this mock
// and maybe do some actual things that help in tests. :)
class FakeEngine : Engine {
    override val version: EngineVersion
        get() = throw NotImplementedError("Not needed for test")

    override fun createView(context: Context, attrs: AttributeSet?): EngineView =
            throw UnsupportedOperationException()

    override fun createSession(private: Boolean, contextId: String?): EngineSession =
            throw UnsupportedOperationException()

    override fun createSessionState(json: JSONObject) = FakeEngineSessionState()

    override fun createSessionStateFrom(reader: JsonReader): EngineSessionState {
        reader.beginObject()
        reader.endObject()
        return FakeEngineSessionState()
    }

    override fun name(): String =
            throw UnsupportedOperationException()

    override fun speculativeConnect(url: String) =
            throw UnsupportedOperationException()

    override val profiler: Profiler
        get() = throw NotImplementedError("Not needed for test")

    override val settings: Settings = DefaultSettings()
}

class FakeEngineSessionState : EngineSessionState {
    override fun writeTo(writer: JsonWriter) {
        writer.beginObject()
        writer.endObject()
    }
}
