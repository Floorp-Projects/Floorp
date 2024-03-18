/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.jexl.ext

import mozilla.components.lib.jexl.value.JexlArray
import mozilla.components.lib.jexl.value.JexlBoolean
import mozilla.components.lib.jexl.value.JexlDouble
import mozilla.components.lib.jexl.value.JexlInteger
import mozilla.components.lib.jexl.value.JexlString

// Kotlin extensions that make it easier to work with Jexl types and values

inline fun <reified T> List<T>.toJexlArray(): JexlArray {
    val values = when (T::class) {
        String::class -> map { JexlString(it as String) }
        Int::class -> map { JexlInteger(it as Int) }
        Double::class -> map { JexlDouble(it as Double) }
        Float::class -> map { JexlDouble((it as Float).toDouble()) }
        Boolean::class -> map { JexlBoolean(it as Boolean) }
        else -> throw UnsupportedOperationException("Can't convert type " + T::class + " to Jexl")
    }

    return JexlArray(values)
}

fun String.toJexl(): JexlString = JexlString(this)
fun Int.toJexl(): JexlInteger = JexlInteger(this)
fun Double.toJexl(): JexlDouble = JexlDouble(this)
fun Float.toJexl(): JexlDouble = JexlDouble(this.toDouble())
fun Boolean.toJexl(): JexlBoolean = JexlBoolean(this)
