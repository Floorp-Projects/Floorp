/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2023-05-12

object FenixVersions {
    const val kotlin = "1.8.21"
    const val coroutines = "1.6.4"

    // These versions are linked: lint should be X+23.Y.Z of gradle_plugin version, according to:
    // https://github.com/alexjlockwood/android-lint-checks-demo/blob/0245fc027463137b1b4afb97c5295d60dce998b6/dependencies.gradle#L3
    const val android_gradle_plugin = "7.4.2"
    const val android_lint_api = "30.4.2"

    const val sentry = "6.19.0"
    const val leakcanary = "2.11"
    const val osslicenses_plugin = "0.10.4"
    const val detekt = "1.22.0"

    const val androidx_activity = "1.7.1"
    const val androidx_compose = "1.4.3"
    const val androidx_compose_compiler = "1.4.7"
    const val androidx_appcompat = "1.6.1"
    const val androidx_benchmark = "1.1.1"
    const val androidx_biometric = "1.1.0"
    const val androidx_coordinator_layout = "1.2.0"
    const val androidx_constraint_layout = "2.1.4"
    const val androidx_preference = "1.1.1"
    const val androidx_profileinstaller = "1.3.1"
    const val androidx_legacy = "1.0.0"
    const val androidx_annotation = "1.6.0"
    const val androidx_lifecycle = "2.6.1"
    const val androidx_fragment = "1.5.7"
    const val androidx_navigation = "2.5.3"
    const val androidx_recyclerview = "1.3.0"
    const val androidx_core = "1.10.0"
    const val androidx_paging = "3.1.1"
    const val androidx_transition = "1.4.1"
    const val androidx_work = "2.7.1"
    const val androidx_datastore = "1.0.0"
    const val androidx_datastore_preferences = "1.0.0"
    const val google_material = "1.9.0"
    const val google_accompanist = "0.30.1"

    const val adjust = "4.33.0"
    const val installreferrer = "2.2"

    const val junit = "5.9.3"
    const val mockk = "1.12.0"

    const val mockwebserver = "4.11.0"
    const val uiautomator = "2.2.0"
    const val robolectric = "4.10.1"

    const val google_ads_id_version = "16.0.0"

    const val google_play_review_version = "2.0.0"

    // keep in sync with the versions used in AS.
    const val protobuf = "3.21.10"
    const val protobuf_plugin = "0.9.3"
}

@Suppress("unused")
object FenixDependencies {
    const val tools_androidgradle = "com.android.tools.build:gradle:${FenixVersions.android_gradle_plugin}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${FenixVersions.kotlin}"
    const val tools_benchmarkgradle = "androidx.benchmark:benchmark-gradle-plugin:${FenixVersions.androidx_benchmark}"
    const val kotlin_reflect = "org.jetbrains.kotlin:kotlin-reflect:${FenixVersions.kotlin}"
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-core:${FenixVersions.coroutines}"
    const val kotlin_coroutines_test = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${FenixVersions.coroutines}"
    const val kotlin_coroutines_android = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${FenixVersions.coroutines}"

    const val osslicenses_plugin = "com.google.android.gms:oss-licenses-plugin:${FenixVersions.osslicenses_plugin}"

    const val sentry = "io.sentry:sentry-android:${FenixVersions.sentry}"
    const val leakcanary = "com.squareup.leakcanary:leakcanary-android-core:${FenixVersions.leakcanary}"

    const val androidx_compose_ui = "androidx.compose.ui:ui:${FenixVersions.androidx_compose}"
    const val androidx_compose_ui_test = "androidx.compose.ui:ui-test-junit4:${FenixVersions.androidx_compose}"
    const val androidx_compose_ui_test_manifest = "androidx.compose.ui:ui-test-manifest:${FenixVersions.androidx_compose}"
    const val androidx_compose_ui_tooling = "androidx.compose.ui:ui-tooling:${FenixVersions.androidx_compose}"
    const val androidx_compose_ui_tooling_preview = "androidx.compose.ui:ui-tooling-preview:${FenixVersions.androidx_compose}"
    const val androidx_compose_foundation = "androidx.compose.foundation:foundation:${FenixVersions.androidx_compose}"
    const val androidx_compose_material = "androidx.compose.material:material:${FenixVersions.androidx_compose}"
    const val androidx_annotation = "androidx.annotation:annotation:${FenixVersions.androidx_annotation}"
    const val androidx_benchmark_junit4 = "androidx.benchmark:benchmark-junit4:${FenixVersions.androidx_benchmark}"
    const val androidx_benchmark_macro_junit4 = "androidx.benchmark:benchmark-macro-junit4:${FenixVersions.androidx_benchmark}"
    const val androidx_profileinstaller = "androidx.profileinstaller:profileinstaller:${FenixVersions.androidx_profileinstaller}"
    const val androidx_biometric = "androidx.biometric:biometric:${FenixVersions.androidx_biometric}"
    const val androidx_fragment = "androidx.fragment:fragment-ktx:${FenixVersions.androidx_fragment}"
    const val androidx_activity_compose = "androidx.activity:activity-compose:${FenixVersions.androidx_activity}"
    const val androidx_activity_ktx = "androidx.activity:activity-ktx:${FenixVersions.androidx_activity}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${FenixVersions.androidx_appcompat}"
    const val androidx_coordinatorlayout = "androidx.coordinatorlayout:coordinatorlayout:${FenixVersions.androidx_coordinator_layout}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${FenixVersions.androidx_constraint_layout}"
    const val androidx_legacy = "androidx.legacy:legacy-support-v4:${FenixVersions.androidx_legacy}"
    const val androidx_lifecycle_common = "androidx.lifecycle:lifecycle-common:${FenixVersions.androidx_lifecycle}"
    const val androidx_lifecycle_livedata = "androidx.lifecycle:lifecycle-livedata-ktx:${FenixVersions.androidx_lifecycle}"
    const val androidx_lifecycle_process = "androidx.lifecycle:lifecycle-process:${FenixVersions.androidx_lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${FenixVersions.androidx_lifecycle}"
    const val androidx_lifecycle_runtime = "androidx.lifecycle:lifecycle-runtime-ktx:${FenixVersions.androidx_lifecycle}"
    const val androidx_paging = "androidx.paging:paging-runtime-ktx:${FenixVersions.androidx_paging}"
    const val androidx_preference = "androidx.preference:preference-ktx:${FenixVersions.androidx_preference}"
    const val androidx_safeargs = "androidx.navigation:navigation-safe-args-gradle-plugin:${FenixVersions.androidx_navigation}"
    const val androidx_navigation_fragment = "androidx.navigation:navigation-fragment-ktx:${FenixVersions.androidx_navigation}"
    const val androidx_navigation_ui = "androidx.navigation:navigation-ui:${FenixVersions.androidx_navigation}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${FenixVersions.androidx_recyclerview}"
    const val androidx_core = "androidx.core:core:${FenixVersions.androidx_core}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${FenixVersions.androidx_core}"
    const val androidx_transition = "androidx.transition:transition:${FenixVersions.androidx_transition}"
    const val androidx_work_ktx = "androidx.work:work-runtime-ktx:${FenixVersions.androidx_work}"
    const val androidx_work_testing = "androidx.work:work-testing:${FenixVersions.androidx_work}"
    const val androidx_datastore = "androidx.datastore:datastore:${FenixVersions.androidx_datastore}"
    const val androidx_data_store_preferences = "androidx.datastore:datastore-preferences:${FenixVersions.androidx_datastore_preferences}"

    const val google_material = "com.google.android.material:material:${FenixVersions.google_material}"
    const val google_accompanist_drawablepainter = "com.google.accompanist:accompanist-drawablepainter:${FenixVersions.google_accompanist}"

    const val protobuf_javalite = "com.google.protobuf:protobuf-javalite:${FenixVersions.protobuf}"
    const val protobuf_compiler = "com.google.protobuf:protoc:${FenixVersions.protobuf}"

    const val adjust = "com.adjust.sdk:adjust-android:${FenixVersions.adjust}"
    const val installreferrer = "com.android.installreferrer:installreferrer:${FenixVersions.installreferrer}"

    const val junit = "junit:junit:${FenixVersions.junit}"
    const val mockk = "io.mockk:mockk:${FenixVersions.mockk}"
    const val mockk_android = "io.mockk:mockk-android:${FenixVersions.mockk}"

    // --- START AndroidX test dependencies --- //
    // N.B.: the versions of these dependencies appear to be pinned together. To avoid bugs, they
    // should always be updated together based on the latest version from the Android test releases page:
    //   https://developer.android.com/jetpack/androidx/releases/test
    // For the full IDs of these test dependencies, see:
    //   https://developer.android.com/training/testing/set-up-project#android-test-dependencies
    private const val androidx_test_shared_version = "1.5.0"
    private const val androidx_test_junit = "1.1.5"
    private const val androidx_test_orchestrator = "1.4.2"
    private const val androidx_test_runner = "1.5.2"
    const val androidx_test_core = "androidx.test:core:$androidx_test_shared_version"
    private const val androidx_espresso_version = "3.5.1"
    const val espresso_core = "androidx.test.espresso:espresso-core:$androidx_espresso_version"
    const val espresso_contrib = "androidx.test.espresso:espresso-contrib:$androidx_espresso_version"
    const val espresso_idling_resources = "androidx.test.espresso:espresso-idling-resource:$androidx_espresso_version"
    const val espresso_intents = "androidx.test.espresso:espresso-intents:$androidx_espresso_version"
    const val androidx_junit = "androidx.test.ext:junit:$androidx_test_junit"
    const val androidx_test_extensions = "androidx.test.ext:junit-ktx:$androidx_test_junit"
    // Monitor is unused
    const val orchestrator = "androidx.test:orchestrator:$androidx_test_orchestrator"
    const val tools_test_runner = "androidx.test:runner:$androidx_test_runner"
    const val tools_test_rules = "androidx.test:rules:$androidx_test_shared_version"
    // Truth is unused
    // Test services is unused
    // --- END AndroidX test dependencies --- //

    const val mockwebserver = "com.squareup.okhttp3:mockwebserver:${FenixVersions.mockwebserver}"
    const val uiautomator = "androidx.test.uiautomator:uiautomator:${FenixVersions.uiautomator}"
    const val robolectric = "org.robolectric:robolectric:${FenixVersions.robolectric}"

    const val google_ads_id = "com.google.android.gms:play-services-ads-identifier:${FenixVersions.google_ads_id_version}"

    // Required for in-app reviews
    const val google_play_review = "com.google.android.play:review:${FenixVersions.google_play_review_version}"
    const val google_play_review_ktx = "com.google.android.play:review-ktx:${FenixVersions.google_play_review_version}"

    const val detektApi = "io.gitlab.arturbosch.detekt:detekt-api:${FenixVersions.detekt}"
    const val detektTest = "io.gitlab.arturbosch.detekt:detekt-test:${FenixVersions.detekt}"
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
