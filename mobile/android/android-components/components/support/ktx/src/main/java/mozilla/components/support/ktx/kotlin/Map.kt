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
@Suppress("ComplexMethod")
fun <K, V> Map<K, V>.toBundle(): Bundle {
    return Bundle().apply {
        forEach { (k, v) ->
            val key = k.toString()

            when (v) {
                is IBinder -> putBinder(key, v)
                is Boolean -> putBoolean(key, v)
                is BooleanArray -> putBooleanArray(key, v)
                is Bundle -> putBundle(key, v)
                is Byte -> putByte(key, v)
                is ByteArray -> putByteArray(key, v)
                is Char -> putChar(key, v)
                is CharArray -> putCharArray(key, v)
                is CharSequence -> putCharSequence(key, v)
                is Double -> putDouble(key, v)
                is DoubleArray -> putDoubleArray(key, v)
                is Float -> putFloat(key, v)
                is FloatArray -> putFloatArray(key, v)
                is Int -> putInt(key, v)
                is IntArray -> putIntArray(key, v)
                is Long -> putLong(key, v)
                is LongArray -> putLongArray(key, v)
                is Parcelable -> putParcelable(key, v)
                is Serializable -> putSerializable(key, v)
                is Short -> putShort(key, v)
                is ShortArray -> putShortArray(key, v)
                is Size -> putSize(key, v)
                is SizeF -> putSizeF(key, v)
            }
        }
    }
}
