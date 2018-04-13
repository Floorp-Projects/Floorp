/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlin

import android.net.Uri
import android.text.TextUtils

/**
 * Normalizes a URL string.
 */
fun String.toNormalizedUrl(): String {
    val trimmedInput = this.trim({ it <= ' ' })
    var uri = Uri.parse(trimmedInput)
    if (TextUtils.isEmpty(uri.scheme)) {
        uri = Uri.parse("http://" + trimmedInput)
    }
    return uri.toString()
}
