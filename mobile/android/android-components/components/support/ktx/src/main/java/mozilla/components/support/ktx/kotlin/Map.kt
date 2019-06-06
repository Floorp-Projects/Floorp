/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.os.Bundle
import androidx.core.os.bundleOf

/**
 * Converts a Map to a Bundle
 */
@Suppress("SpreadOperator")
@Deprecated("Use Android KTX bundleOf()")
fun <K, V> Map<K, V>.toBundle(): Bundle {
    val pairs = mapKeys { (key) -> key.toString() }.toList().toTypedArray()
    return bundleOf(*pairs)
}
