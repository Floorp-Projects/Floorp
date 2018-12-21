/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.utils

object StringUtils {
    fun safeSubstring(str: String, start: Int, end: Int): String {
        return str.substring(
            Math.max(0, start),
            Math.min(end, str.length)
        )
    }
}
