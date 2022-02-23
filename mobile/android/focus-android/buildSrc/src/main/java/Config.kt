/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Project

object Config {
    @JvmStatic
    fun releaseVersionName(project: Project): String? {
        // Note: release builds must have the `versionName` set. However, the gradle ecosystem makes this hard to
        // ergonomically validate (sometimes IDEs default to a release variant and mysteriously fail due to the
        // validation, sometimes devs just need a release build and specifying project properties is annoying in IDEs),
        // so instead we'll allow the `versionName` to silently default to an empty string.
        return if (project.hasProperty("versionName")) project.property("versionName") as String else null
    }

    @JvmStatic
    fun nightlyVersionName(): String {
        // Nightly versions use the Gecko/A-C major version and append "0.a1", e.g. with A-C 90.0.20210426143115
        // the Nightly version will be 90.0a1
        val majorVersion = AndroidComponents.VERSION.split(".")[0]
        return "$majorVersion.0a1"
    }
}
