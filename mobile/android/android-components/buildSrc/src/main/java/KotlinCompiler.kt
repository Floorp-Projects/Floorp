/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A collection of functions and properties to interact with the Kotlin compiler.
 */
object KotlinCompiler {

    // Ideally, this would be a function to enable warningsAsErrors. However, it's hard to make the
    // KotlinCompile task dependencies available to buildSrc so we settle for defining this list here instead.
    // Maybe this is easier in Gradle 5+.
    @JvmStatic
    val projectsWithWarningsAsErrorsDisabled = setOf(
        "browser-domains",
        "browser-engine-servo",
        "browser-storage-sync",
        "feature-accounts",
        "feature-contextmenu",
        "feature-downloads",
        "feature-prompts",
        "feature-search",
        "feature-sitepermissions",
        "feature-tabs",
        "service-glean",
        "support-test",
        "ui-tabcounter"
    )
}
