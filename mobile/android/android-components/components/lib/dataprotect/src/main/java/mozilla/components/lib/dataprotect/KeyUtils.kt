/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.dataprotect

import java.security.SecureRandom

/**
 * Generates a random key of specified [keyStrength].
 *
 * @param keyStrength The strength of the generated key in bits
 */
@Suppress("MagicNumber")
fun generateEncryptionKey(keyStrength: Int): String {
    require(keyStrength >= 256) { "Key strength must be at least 256 bits" }
    val bytes = ByteArray(keyStrength / 8)
    val random = SecureRandom()
    random.nextBytes(bytes)

    return bytes
        .map {
            val full = it.toInt()
            val hi = (full and 0xf0) ushr 4
            val lo = full and 0x0f
            "%x%x".format(hi, lo)
        }
        .joinToString("")
}
