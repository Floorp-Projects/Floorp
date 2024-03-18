/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import android.util.JsonReader
import android.util.JsonToken

/**
 * Returns the [JsonToken.STRING] value of the next token or `null` if the next token
 * is [JsonToken.NULL].
 */
fun JsonReader.nextStringOrNull(): String? {
    return if (peek() == JsonToken.NULL) {
        nextNull()
        null
    } else {
        nextString()
    }
}

/**
 * Returns the [JsonToken.BOOLEAN] value of the next token or `null` if the next token
 * is [JsonToken.NULL].
 */
fun JsonReader.nextBooleanOrNull(): Boolean? {
    return if (peek() == JsonToken.NULL) {
        nextNull()
        null
    } else {
        nextBoolean()
    }
}

/**
 * Returns the [JsonToken.NUMBER] value of the next token or `null` if the next token
 * is [JsonToken.NULL].
 */
fun JsonReader.nextIntOrNull(): Int? {
    return if (peek() == JsonToken.NULL) {
        nextNull()
        null
    } else {
        nextInt()
    }
}
