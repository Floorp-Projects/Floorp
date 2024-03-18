/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.whatsnew

import android.content.Context
import mozilla.components.support.ktx.android.content.appVersionName

// This file is a modified port from Focus Android

/**
 * Convenience class to deal with the application version number
 * I opted to keep it contained to the whatsnew package. We may
 * want to pull it
 */
open class WhatsNewVersion(internal open val version: String) {

    override fun hashCode(): Int {
        return version.hashCode()
    }

    override fun equals(other: Any?): Boolean {
        if (other is WhatsNewVersion) {
            return version == other.version
        }

        return false
    }

    // Splitting on a dot to get the major version number fails on nightly builds, so we
    // return 0 in those cases
    val majorVersionNumber: Int
        get() = version.split(".").first().toIntOrNull() ?: 0
}

data class ContextWhatsNewVersion(private val context: Context) : WhatsNewVersion("") {
    override val version: String
        get() = context.appVersionName
}
