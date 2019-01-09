/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Synchronized library configuration for all modules
// This "componentsVersion" number is defined in ".buildconfig.yml" and should follow
// semantic versioning (MAJOR.MINOR.PATCH). See https://semver.org/
class Config(val componentsVersion: String) {
    // Maven group ID used for all components
    val componentsGroupId = "org.mozilla.components"

    // Synchronized build configuration for all modules
    val compileSdkVersion = 28
    val minSdkVersion = 21
    val targetSdkVersion = 28

    // Component lib-dataprotect requires functionality from API 23.
    val minSdkVersion_dataprotect = 23
}
