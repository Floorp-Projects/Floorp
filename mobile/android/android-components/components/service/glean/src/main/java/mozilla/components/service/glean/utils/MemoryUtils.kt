/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.utils

import mozilla.components.service.glean.private.MemoryUnit

/**
 * Convenience method to convert a memory size in a different unit to bytes.
 *
 * @param memoryUnit the unit the value is in
 * @param value a memory size in the given unit
 *
 * @return the memory size, in bytes
 */
@Suppress("MagicNumber")
internal fun memoryToBytes(memoryUnit: MemoryUnit, value: Long): Long {
    return when (memoryUnit) {
        MemoryUnit.Byte -> value
        MemoryUnit.Kilobyte -> value shl 10
        MemoryUnit.Megabyte -> value shl 20
        MemoryUnit.Gigabyte -> value shl 30
    }
}
