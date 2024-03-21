/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import org.gradle.api.Plugin
import org.gradle.api.initialization.Settings

// If you ever need to force a toolchain rebuild (taskcluster) then edit the following comment.
// FORCE REBUILD 2023-05-24

class DependenciesPlugin : Plugin<Settings> {
    override fun apply(settings: Settings) = Unit
}

// Synchronized version numbers for dependencies used by (some) modules
object Versions {
    const val kotlin = "1.9.23"
    const val coroutines = "1.8.0"
    const val serialization = "1.6.3"
    const val python_envs_plugin = "0.0.31"

    const val mozilla_glean = "58.1.0"

    const val junit = "4.13.2"
    const val robolectric = "4.11.1"
    const val mockito = "5.11.0"
    const val maven_ant_tasks = "2.1.3"
    const val jacoco = "0.8.11"
    const val okhttp = "4.12.0"
    const val okio = "3.8.0"
    const val androidsvg = "1.4"

    const val android_gradle_plugin = "8.3.0"

    // This has to be synced to the gradlew plugin version. See
    // http://googlesamples.github.io/android-custom-lint-rules/api-guide/example.md.html#example:samplelintcheckgithubproject/lintversion?
    const val lint = "31.3.0"
    const val detekt = "1.23.5"
    const val ktlint = "0.49.1"

    const val sentry = "7.5.0"

    const val zxing = "3.5.3"

    const val disklrucache = "2.0.2"
    const val leakcanary = "2.13"

    const val material = "1.9.0"
    const val ksp = "1.0.19"
    val ksp_plugin = "$kotlin-$ksp"

    // see https://android-developers.googleblog.com/2022/06/independent-versioning-of-Jetpack-Compose-libraries.html
    // for Jetpack Compose libraries versioning
    const val compose_compiler = "1.5.11"

    object AndroidX {
        const val activityCompose = "1.7.2"
        const val annotation = "1.7.1"
        const val appcompat = "1.6.1"
        const val autofill = "1.1.0"
        const val browser = "1.8.0"
        const val biometric = "1.1.0"
        const val cardview = "1.0.0"
        const val compose_bom = "2023.10.01"
        const val constraintlayout = "2.1.4"
        const val coordinatorlayout = "1.2.0"
        const val core = "1.12.0"
        const val drawerlayout = "1.2.0"
        const val fragment = "1.6.2"
        const val recyclerview = "1.3.2"
        const val test = "1.5.0"
        const val test_ext = "1.1.5"
        const val test_runner = "1.5.2"
        const val espresso = "3.5.1"
        const val room = "2.6.1"
        const val savedstate = "1.2.1"
        const val paging = "3.2.1"
        const val palette = "1.0.0"
        const val preferences = "1.2.1"
        const val lifecycle = "2.7.0"
        const val media = "1.7.0"
        const val navigation = "2.7.7"
        const val work = "2.9.0"
        const val arch = "2.2.0"
        const val uiautomator = "2.3.0"
        const val localbroadcastmanager = "1.0.0"
        const val swiperefreshlayout = "1.1.0"
        const val data_store_preferences="1.0.0"
    }

    object Firebase {
        const val messaging = "23.4.1"
    }
}

// Synchronized dependencies used by (some) modules
@Suppress("Unused", "MaxLineLength")
object ComponentsDependencies {
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.coroutines}"
    const val kotlin_reflect = "org.jetbrains.kotlin:kotlin-reflect:${Versions.kotlin}"
    const val kotlin_json = "org.jetbrains.kotlinx:kotlinx-serialization-json:${Versions.serialization}"

    const val testing_junit = "junit:junit:${Versions.junit}"
    const val testing_robolectric = "org.robolectric:robolectric:${Versions.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${Versions.mockito}"
    const val testing_mockwebserver = "com.squareup.okhttp3:mockwebserver:${Versions.okhttp}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${Versions.coroutines}"
    const val testing_maven_ant_tasks = "org.apache.maven:maven-ant-tasks:${Versions.maven_ant_tasks}"
    const val testing_leakcanary = "com.squareup.leakcanary:leakcanary-android-instrumentation:${Versions.leakcanary}"

    const val androidx_activity_compose = "androidx.activity:activity-compose:${Versions.AndroidX.activityCompose}"
    const val androidx_annotation = "androidx.annotation:annotation:${Versions.AndroidX.annotation}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.AndroidX.appcompat}"
    const val androidx_autofill = "androidx.autofill:autofill:${Versions.AndroidX.autofill}"
    const val androidx_arch_core_common = "androidx.arch.core:core-common:${Versions.AndroidX.arch}"
    const val androidx_arch_core_testing = "androidx.arch.core:core-testing:${Versions.AndroidX.arch}"
    const val androidx_biometric = "androidx.biometric:biometric:${Versions.AndroidX.biometric}"
    const val androidx_browser = "androidx.browser:browser:${Versions.AndroidX.browser}"
    const val androidx_cardview = "androidx.cardview:cardview:${Versions.AndroidX.cardview}"

    const val androidx_compose_bom = "androidx.compose:compose-bom:${Versions.AndroidX.compose_bom}"
    const val androidx_compose_animation = "androidx.compose.animation:animation"
    const val androidx_compose_ui = "androidx.compose.ui:ui"
    const val androidx_compose_ui_graphics = "androidx.compose.ui:ui-graphics"
    const val androidx_compose_ui_test = "androidx.compose.ui:ui-test-junit4"
    const val androidx_compose_ui_test_manifest = "androidx.compose.ui:ui-test-manifest"
    const val androidx_compose_ui_tooling = "androidx.compose.ui:ui-tooling"
    const val androidx_compose_ui_tooling_preview = "androidx.compose.ui:ui-tooling-preview"
    const val androidx_compose_foundation = "androidx.compose.foundation:foundation"
    const val androidx_compose_material = "androidx.compose.material:material"
    const val androidx_compose_runtime_livedata = "androidx.compose.runtime:runtime-livedata"

    const val androidx_safeargs = "androidx.navigation:navigation-safe-args-gradle-plugin:${Versions.AndroidX.navigation}"
    const val androidx_navigation_fragment = "androidx.navigation:navigation-fragment-ktx:${Versions.AndroidX.navigation}"
    const val androidx_navigation_ui = "androidx.navigation:navigation-ui:${Versions.AndroidX.navigation}"
    const val androidx_compose_navigation = "androidx.navigation:navigation-compose:${Versions.AndroidX.navigation}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${Versions.AndroidX.constraintlayout}"
    const val androidx_core = "androidx.core:core:${Versions.AndroidX.core}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.AndroidX.core}"
    const val androidx_coordinatorlayout = "androidx.coordinatorlayout:coordinatorlayout:${Versions.AndroidX.coordinatorlayout}"
    const val androidx_drawerlayout = "androidx.drawerlayout:drawerlayout:${Versions.AndroidX.drawerlayout}"
    const val androidx_fragment = "androidx.fragment:fragment:${Versions.AndroidX.fragment}"
    const val androidx_lifecycle_common = "androidx.lifecycle:lifecycle-common:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_livedata = "androidx.lifecycle:lifecycle-livedata-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_process = "androidx.lifecycle:lifecycle-process:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_runtime = "androidx.lifecycle:lifecycle-runtime-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_service = "androidx.lifecycle:lifecycle-service:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_media = "androidx.media:media:${Versions.AndroidX.media}"
    const val androidx_paging = "androidx.paging:paging-runtime:${Versions.AndroidX.paging}"
    const val androidx_palette = "androidx.palette:palette-ktx:${Versions.AndroidX.palette}"
    const val androidx_preferences = "androidx.preference:preference:${Versions.AndroidX.preferences}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.AndroidX.recyclerview}"
    const val androidx_room_runtime = "androidx.room:room-ktx:${Versions.AndroidX.room}"
    const val androidx_room_compiler = "androidx.room:room-compiler:${Versions.AndroidX.room}"
    const val androidx_room_testing = "androidx.room:room-testing:${Versions.AndroidX.room}"
    const val androidx_savedstate = "androidx.savedstate:savedstate:${Versions.AndroidX.savedstate}"
    const val androidx_test_core = "androidx.test:core-ktx:${Versions.AndroidX.test}"
    const val androidx_test_junit = "androidx.test.ext:junit-ktx:${Versions.AndroidX.test_ext}"
    const val androidx_test_runner = "androidx.test:runner:${Versions.AndroidX.test_runner}"
    const val androidx_test_rules = "androidx.test:rules:${Versions.AndroidX.test}"
    const val androidx_test_uiautomator = "androidx.test.uiautomator:uiautomator:${Versions.AndroidX.uiautomator}"
    const val androidx_work_runtime = "androidx.work:work-runtime:${Versions.AndroidX.work}"
    const val androidx_work_testing = "androidx.work:work-testing:${Versions.AndroidX.work}"
    const val androidx_espresso_core = "androidx.test.espresso:espresso-core:${Versions.AndroidX.espresso}"
    const val androidx_localbroadcastmanager = "androidx.localbroadcastmanager:localbroadcastmanager:${Versions.AndroidX.localbroadcastmanager}"
    const val androidx_swiperefreshlayout = "androidx.swiperefreshlayout:swiperefreshlayout:${Versions.AndroidX.swiperefreshlayout}"
    const val androidx_data_store_preferences = "androidx.datastore:datastore-preferences:${Versions.AndroidX.data_store_preferences}"

    const val google_material = "com.google.android.material:material:${Versions.material}"

    const val plugin_serialization = "org.jetbrains.kotlin.plugin.serialization:org.jetbrains.kotlin.plugin.serialization.gradle.plugin:${Versions.kotlin}"

    const val leakcanary = "com.squareup.leakcanary:leakcanary-android:${Versions.leakcanary}"

    const val tools_androidgradle = "com.android.tools.build:gradle:${Versions.android_gradle_plugin}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.kotlin}"

    const val tools_lint = "com.android.tools.lint:lint:${Versions.lint}"
    const val tools_lintapi = "com.android.tools.lint:lint-api:${Versions.lint}"
    const val tools_lintchecks = "com.android.tools.lint:lint-checks:${Versions.lint}"
    const val tools_linttests = "com.android.tools.lint:lint-tests:${Versions.lint}"

    const val tools_detekt_api = "io.gitlab.arturbosch.detekt:detekt-api:${Versions.detekt}"
    const val tools_detekt_test = "io.gitlab.arturbosch.detekt:detekt-test:${Versions.detekt}"

    val mozilla_appservices_fxaclient = "${ApplicationServicesConfig.groupId}:fxaclient:${ApplicationServicesConfig.version}"
    val mozilla_appservices_nimbus = "${ApplicationServicesConfig.groupId}:nimbus:${ApplicationServicesConfig.version}"
    const val mozilla_glean_forUnitTests = "org.mozilla.telemetry:glean-native-forUnitTests:${Versions.mozilla_glean}"
    val mozilla_appservices_autofill = "${ApplicationServicesConfig.groupId}:autofill:${ApplicationServicesConfig.version}"
    val mozilla_appservices_logins = "${ApplicationServicesConfig.groupId}:logins:${ApplicationServicesConfig.version}"
    val mozilla_appservices_places = "${ApplicationServicesConfig.groupId}:places:${ApplicationServicesConfig.version}"
    val mozilla_appservices_syncmanager = "${ApplicationServicesConfig.groupId}:syncmanager:${ApplicationServicesConfig.version}"
    val mozilla_remote_settings = "${ApplicationServicesConfig.groupId}:remotesettings:${ApplicationServicesConfig.version}"
    val mozilla_appservices_push = "${ApplicationServicesConfig.groupId}:push:${ApplicationServicesConfig.version}"
    val mozilla_appservices_tabs = "${ApplicationServicesConfig.groupId}:tabs:${ApplicationServicesConfig.version}"
    val mozilla_appservices_suggest = "${ApplicationServicesConfig.groupId}:suggest:${ApplicationServicesConfig.version}"
    val mozilla_appservices_httpconfig = "${ApplicationServicesConfig.groupId}:httpconfig:${ApplicationServicesConfig.version}"
    val mozilla_appservices_full_megazord = "${ApplicationServicesConfig.groupId}:full-megazord:${ApplicationServicesConfig.version}"
    val mozilla_appservices_full_megazord_forUnitTests = "${ApplicationServicesConfig.groupId}:full-megazord-forUnitTests:${ApplicationServicesConfig.version}"

    val mozilla_appservices_errorsupport = "${ApplicationServicesConfig.groupId}:errorsupport:${ApplicationServicesConfig.version}"
    val mozilla_appservices_rust_log_forwarder = "${ApplicationServicesConfig.groupId}:rust-log-forwarder:${ApplicationServicesConfig.version}"
    val mozilla_appservices_sync15 = "${ApplicationServicesConfig.groupId}:sync15:${ApplicationServicesConfig.version}"

    const val thirdparty_okhttp = "com.squareup.okhttp3:okhttp:${Versions.okhttp}"
    const val thirdparty_okhttp_urlconnection = "com.squareup.okhttp3:okhttp-urlconnection:${Versions.okhttp}"
    const val thirdparty_okio = "com.squareup.okio:okio:${Versions.okio}"
    const val thirdparty_sentry = "io.sentry:sentry-android:${Versions.sentry}"
    const val thirdparty_zxing = "com.google.zxing:core:${Versions.zxing}"
    const val thirdparty_disklrucache = "com.jakewharton:disklrucache:${Versions.disklrucache}"
    const val thirdparty_androidsvg = "com.caverock:androidsvg-aar:${Versions.androidsvg}"

    const val firebase_messaging = "com.google.firebase:firebase-messaging:${Versions.Firebase.messaging}"
}
