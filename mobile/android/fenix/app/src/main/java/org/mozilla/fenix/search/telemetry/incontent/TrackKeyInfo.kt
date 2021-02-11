/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search.telemetry.incontent

import java.util.Locale

internal data class TrackKeyInfo(
    var providerName: String,
    var type: String,
    var code: String?,
    var channel: String? = null
) {
    fun createTrackKey(): String {
        return "${providerName.toLowerCase(Locale.ROOT)}.in-content" +
                ".${type.toLowerCase(Locale.ROOT)}" +
                ".${code?.toLowerCase(Locale.ROOT) ?: "none"}" +
                if (!channel?.toLowerCase(Locale.ROOT).isNullOrBlank())
                    ".${channel?.toLowerCase(Locale.ROOT)}"
                else ""
    }
}
