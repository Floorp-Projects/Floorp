/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.fakes.engine

import android.util.JsonWriter
import mozilla.components.concept.engine.EngineSessionState

/**
 * A fake [EngineSessionState] that can be used with [FakeEngine] in tests.
 */
class FakeEngineSessionState(
    val value: String,
) : EngineSessionState {
    override fun writeTo(writer: JsonWriter) {
        writer.beginObject()

        writer.name("engine")
        writer.value(value)

        writer.endObject()
    }
}
