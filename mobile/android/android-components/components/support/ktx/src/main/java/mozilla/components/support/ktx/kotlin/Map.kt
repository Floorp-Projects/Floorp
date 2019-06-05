/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.os.Bundle
import android.os.IBinder
import android.os.Parcelable
import android.util.Size
import android.util.SizeF
import java.io.Serializable

/**
 * Converts a Map to a Bundle
 */
fun <K, V> Map<K, V>.toBundle(): Bundle {
    return Bundle().apply {
        forEach { (k, v) ->
            val key = k.toString()
            put(key, v)
        }
    }
}

private fun <V> Bundle.put(key: String, value: V) {
    when (value) {
        is IBinder -> putBinder(key, value)
        is Boolean -> putBoolean(key, value)
        is BooleanArray -> putBooleanArray(key, value)
        is Bundle -> putBundle(key, value)
        is Byte -> putByte(key, value)
        is ByteArray -> putByteArray(key, value)
        is Char -> putChar(key, value)
        is CharArray -> putCharArray(key, value)
        is CharSequence -> putCharSequence(key, value)
        is Double -> putDouble(key, value)
        is DoubleArray -> putDoubleArray(key, value)
        is Float -> putFloat(key, value)
        is FloatArray -> putFloatArray(key, value)
        is Int -> putInt(key, value)
        is IntArray -> putIntArray(key, value)
        is Long -> putLong(key, value)
        is LongArray -> putLongArray(key, value)
        is Parcelable -> putParcelable(key, value)
        is Serializable -> putSerializable(key, value)
        is Short -> putShort(key, value)
        is ShortArray -> putShortArray(key, value)
        is Size -> putSize(key, value)
        is SizeF -> putSizeF(key, value)
    }
}
