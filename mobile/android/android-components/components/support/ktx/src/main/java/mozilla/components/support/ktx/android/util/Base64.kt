/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.util

import android.util.Base64

object Base64 {
    fun encodeToUriString(data: String) =
        "data:text/html;base64," + Base64.encodeToString(data.toByteArray(), Base64.DEFAULT)
}
