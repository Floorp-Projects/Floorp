import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Config {
    const val versionCode = 1
    const val versionName = "1.0"

    // Synchronized build configuration for all modules
    const val compileSdkVersion = 28
    const val minSdkVersion = 21
    const val targetSdkVersion = 28

    @JvmStatic
    fun generateVersionSuffix(): String {
        val today = Date()
        // Append the year (2 digits) and week in year (2 digits). This will make it easier to distinguish versions and
        // identify ancient versions when debugging issues. However this will still keep the same version number during
        // the week so that we do not end up with a lot of versions in tools like Sentry. As an extra this matches the
        // sections we use in the changelog (weeks).
        return SimpleDateFormat(".yyww", Locale.US).format(today)
    }
}
