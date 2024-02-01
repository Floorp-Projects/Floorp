/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2023-05-12

class FenixDependenciesPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

object FenixVersions {
    const val osslicenses_plugin = "0.10.4"

    const val falcon = "2.2.0"
    const val fastlane = "2.1.1"

    const val androidx_activity = "1.7.2"
    const val androidx_benchmark = "1.2.2"
    const val androidx_profileinstaller = "1.3.1"
    const val androidx_legacy = "1.0.0"
    const val androidx_navigation = "2.5.3"
    const val androidx_splash_screen = "1.0.1"
    const val androidx_transition = "1.4.1"
    const val androidx_datastore = "1.0.0"
    const val google_accompanist = "0.30.1"

    const val adjust = "4.38.1"
    const val installreferrer = "2.2"

    const val junit = "5.9.3"
    const val mockk = "1.13.8"

    const val google_ads_id_version = "16.0.0"

    const val google_play_review_version = "2.0.1"

    // keep in sync with the versions used in AS.
    const val protobuf = "3.21.10"
    const val protobuf_plugin = "0.9.4"
}

@Suppress("unused")
object FenixDependencies {
    const val tools_benchmarkgradle = "androidx.benchmark:benchmark-gradle-plugin:${FenixVersions.androidx_benchmark}"

    const val osslicenses_plugin = "com.google.android.gms:oss-licenses-plugin:${FenixVersions.osslicenses_plugin}"

    const val androidx_benchmark_junit4 = "androidx.benchmark:benchmark-junit4:${FenixVersions.androidx_benchmark}"
    const val androidx_benchmark_macro_junit4 = "androidx.benchmark:benchmark-macro-junit4:${FenixVersions.androidx_benchmark}"
    const val androidx_core_splashscreen = "androidx.core:core-splashscreen:${FenixVersions.androidx_splash_screen}"
    const val androidx_profileinstaller = "androidx.profileinstaller:profileinstaller:${FenixVersions.androidx_profileinstaller}"
    const val androidx_activity_ktx = "androidx.activity:activity-ktx:${FenixVersions.androidx_activity}"
    const val androidx_legacy = "androidx.legacy:legacy-support-v4:${FenixVersions.androidx_legacy}"
    const val androidx_safeargs = "androidx.navigation:navigation-safe-args-gradle-plugin:${FenixVersions.androidx_navigation}"
    const val androidx_navigation_fragment = "androidx.navigation:navigation-fragment-ktx:${FenixVersions.androidx_navigation}"
    const val androidx_navigation_ui = "androidx.navigation:navigation-ui:${FenixVersions.androidx_navigation}"
    const val androidx_transition = "androidx.transition:transition:${FenixVersions.androidx_transition}"
    const val androidx_datastore = "androidx.datastore:datastore:${FenixVersions.androidx_datastore}"

    const val google_accompanist_drawablepainter = "com.google.accompanist:accompanist-drawablepainter:${FenixVersions.google_accompanist}"

    const val protobuf_javalite = "com.google.protobuf:protobuf-javalite:${FenixVersions.protobuf}"
    const val protobuf_compiler = "com.google.protobuf:protoc:${FenixVersions.protobuf}"

    const val adjust = "com.adjust.sdk:adjust-android:${FenixVersions.adjust}"
    const val installreferrer = "com.android.installreferrer:installreferrer:${FenixVersions.installreferrer}"

    const val mockk = "io.mockk:mockk:${FenixVersions.mockk}"
    const val mockk_android = "io.mockk:mockk-android:${FenixVersions.mockk}"
    const val falcon = "com.jraska:falcon:${FenixVersions.falcon}"
    const val fastlane = "tools.fastlane:screengrab:${FenixVersions.fastlane}"

    // --- START AndroidX test dependencies --- //
    // N.B.: the versions of these dependencies appear to be pinned together. To avoid bugs, they
    // should always be updated together based on the latest version from the Android test releases page:
    //   https://developer.android.com/jetpack/androidx/releases/test
    // For the full IDs of these test dependencies, see:
    //   https://developer.android.com/training/testing/set-up-project#android-test-dependencies
    private const val androidx_test_orchestrator = "1.4.2"
    private const val androidx_espresso_version = "3.5.1"
    const val espresso_contrib = "androidx.test.espresso:espresso-contrib:$androidx_espresso_version"
    const val espresso_idling_resources = "androidx.test.espresso:espresso-idling-resource:$androidx_espresso_version"
    const val espresso_intents = "androidx.test.espresso:espresso-intents:$androidx_espresso_version"
    // Monitor is unused
    const val orchestrator = "androidx.test:orchestrator:$androidx_test_orchestrator"
    // Truth is unused
    // Test services is unused
    // --- END AndroidX test dependencies --- //

    const val google_ads_id = "com.google.android.gms:play-services-ads-identifier:${FenixVersions.google_ads_id_version}"

    // Required for in-app reviews
    const val google_play_review = "com.google.android.play:review:${FenixVersions.google_play_review_version}"
    const val google_play_review_ktx = "com.google.android.play:review-ktx:${FenixVersions.google_play_review_version}"

    const val junitApi = "org.junit.jupiter:junit-jupiter-api:${FenixVersions.junit}"
    const val junitParams = "org.junit.jupiter:junit-jupiter-params:${FenixVersions.junit}"
    const val junitEngine = "org.junit.jupiter:junit-jupiter-engine:${FenixVersions.junit}"
}

/**
 * Functionality to limit specific dependencies to specific repositories. These are typically expected to be used by
 * dependency group name (i.e. with `include/excludeGroup`). For additional info, see:
 * https://docs.gradle.org/current/userguide/declaring_repositories.html#sec::matching_repositories_to_dependencies
 *
 * Note: I wanted to nest this in Deps but for some reason gradle can't find it so it's top-level now. :|
 */
object RepoMatching {
    const val mozilla = "org\\.mozilla\\..*"
    const val androidx = "androidx\\..*"
    const val comAndroid = "com\\.android.*"
    const val comGoogle = "com\\.google\\..*"
}
