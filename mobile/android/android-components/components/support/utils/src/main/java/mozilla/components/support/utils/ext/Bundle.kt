/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.os.Build
import android.os.Bundle
import android.os.Parcelable
import java.io.Serializable
import java.util.ArrayList

/**
 * Retrieve extended data from the bundle.
 */
fun <T> Bundle.getParcelableCompat(name: String, clazz: Class<T>): T? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getParcelable(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getParcelable(name) as? T?
    }
}

/**
 * Retrieve extended data from the bundle.
 */
fun <T : Serializable> Bundle.getSerializableCompat(name: String, clazz: Class<T>): Serializable? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getSerializable(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getSerializable(name)
    }
}

/**
 * Retrieve extended data from the bundle.
 */
fun <T : Parcelable> Bundle.getParcelableArrayListCompat(name: String, clazz: Class<T>): ArrayList<T>? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getParcelableArrayList(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getParcelableArrayList(name)
    }
}

/**
 * Retrieve extended data from the bundle.
 */
inline fun <reified T : Parcelable> Bundle.getParcelableArrayCompat(name: String, clazz: Class<T>): Array<T>? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getParcelableArray(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getParcelableArray(name)?.safeCastToArrayOfT()
    }
}

/**
 * Cast a [Parcelable] [Array] to a <T implements [Parcelable]> [Array]
 */
inline fun <reified T : Parcelable> Array<Parcelable>.safeCastToArrayOfT(): Array<T> {
    return filterIsInstance<T>().toTypedArray()
}
