/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.browser.engine.servo

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class ServoEngineSessionStateTest {

    @Test
    fun `toJson() returns empty object`() {
        // Given
        val state = ServoEngineSessionState()

        // When
        val json = state.toJSON()

        // Then
        assertEquals("Json object is empty", 0, json.length())
    }

    @Test
    fun `fromJson() returns default state`() {
        // Given
        val json = JSONObject().apply {
            put("some-key", "some-value")
        }

        // When
        val state = ServoEngineSessionState.fromJSON(json)

        // Then
        assertEquals(state, ServoEngineSessionState())
    }
}