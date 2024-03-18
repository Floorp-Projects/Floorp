/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import android.content.Intent
import android.os.Build
import android.os.Parcelable
import java.io.Serializable
import java.util.ArrayList

/**
 * Retrieve extended data from the intent.
 */
fun <T> Intent.getParcelableExtraCompat(name: String, clazz: Class<T>): T? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getParcelableExtra(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getParcelableExtra(name) as? T?
    }
}

/**
 * Retrieve extended data from the intent.
 */
fun <T : Parcelable> Intent.getParcelableArrayListExtraCompat(name: String, clazz: Class<T>): ArrayList<T>? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getParcelableArrayListExtra(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getParcelableArrayListExtra(name)
    }
}

/**
 * Retrieve extended data from the intent.
 */
fun <T : Serializable> Intent.getSerializableExtraCompat(name: String, clazz: Class<T>): Serializable? {
    return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        getSerializableExtra(name, clazz)
    } else {
        @Suppress("DEPRECATION")
        getSerializableExtra(name)
    }
}
