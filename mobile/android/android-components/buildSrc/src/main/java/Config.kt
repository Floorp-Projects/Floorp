/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Config {
    // Synchronized library configuration for all modules
    const val componentsVersion = "0.21"

    // Synchronized build configuration for all modules
    const val compileSdkVersion = 27
    const val minSdkVersion = 21
    const val targetSdkVersion = 27

    // Component lib-dataprotect requires functionality from API 23.
    const val minSdkVersion_dataprotect = 23
}
