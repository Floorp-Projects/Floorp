/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import android.os.Bundle

/**
 * Returns `true` if the two specified bundles are *structurally* equal to one another,
 * i.e. contain the same number of the same elements in the same order.
 */
@Suppress("ComplexMethod")
infix fun Bundle.contentEquals(other: Bundle): Boolean {
    if (size() != other.size()) return false

    return keySet().all { key ->
        val valueTwo = other.get(key)
        when (val valueOne = get(key)) {
            // Compare bundles deeply
            is Bundle -> valueTwo is Bundle && valueOne contentEquals valueTwo

            // Compare arrays using contentEquals
            is BooleanArray -> valueTwo is BooleanArray && valueOne contentEquals valueTwo
            is ByteArray -> valueTwo is ByteArray && valueOne contentEquals valueTwo
            is CharArray -> valueTwo is CharArray && valueOne contentEquals valueTwo
            is DoubleArray -> valueTwo is DoubleArray && valueOne contentEquals valueTwo
            is FloatArray -> valueTwo is FloatArray && valueOne contentEquals valueTwo
            is IntArray -> valueTwo is IntArray && valueOne contentEquals valueTwo
            is LongArray -> valueTwo is LongArray && valueOne contentEquals valueTwo
            is ShortArray -> valueTwo is ShortArray && valueOne contentEquals valueTwo
            is Array<*> -> valueTwo is Array<*> && valueOne contentEquals valueTwo

            else -> valueOne == valueTwo
        }
    }
}
