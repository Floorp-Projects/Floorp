/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object FocusVersions {
    object Adjust {
        const val adjust = "4.33.0"
        const val install_referrer = "2.2"
    }

    object AndroidX {
        const val annotation = "1.5.0"
        const val appcompat = "1.5.1"
        const val arch = "2.1.0"
        const val browser = "1.4.0"
        const val cardview = "1.0.0"
        const val compose = "1.3.1"
        const val constraint_layout = "2.1.4"
        const val constraint_layout_compose = "1.0.1"
        const val core = "1.9.0"
        const val fragment = "1.5.4"
        const val lifecycle = "2.5.1"
        const val palette = "1.0.0"
        const val preference = "1.2.0"
        const val recyclerview = "1.2.1"
        const val savedstate = "1.2.0"
        const val splashscreen = "1.0.0"
        const val transition = "1.4.1"
        const val work = "2.7.1"
    }

    object Google {
        const val accompanist = "0.16.1"
        const val compose_compiler = "1.3.2"
        const val material = "1.7.0"
        const val play = "1.10.3"
    }

    object Gradle {
        const val android_plugin = "7.3.0"
        const val kotlin_plugin = Kotlin.compiler
    }

    object Kotlin {
        const val compiler = "1.7.20"
        const val coroutines = "1.6.4"
    }

    object Testing {
        const val androidx_core = "1.5.0"
        const val androidx_espresso = "3.5.0"
        const val androidx_ext_junit = "1.1.4"
        const val androidx_orchestrator = "1.4.2"
        const val androidx_uiautomator = "2.2.0"
        const val falcon = "2.2.0"
        const val fastlane = "2.1.1"
        const val junit = "5.9.1"
        const val ktlint = "0.47.1"
        const val mockito = "4.8.1"
        const val mockwebserver = "4.10.0"
        const val robolectric = "4.9"
    }

    object ThirdParty {
        const val jna = "5.12.1"
        const val leakcanary = "2.10"
        const val sentry = "6.8.0"
    }

    // Workaround for a Gradle parsing bug that prevents using nested objects directly in Gradle files.
    // These might be removable if we switch to kts files instead.
    // https://github.com/gradle/gradle/issues/9251
    const val google_compose_compiler = FocusVersions.Google.compose_compiler
    const val ktlint_version = FocusVersions.Testing.ktlint
}

object FocusDependencies {
    const val androidx_annotation = "androidx.annotation:annotation:${FocusVersions.AndroidX.annotation}"
    const val androidx_arch_core_testing = "androidx.arch.core:core-testing:${FocusVersions.AndroidX.arch}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${FocusVersions.AndroidX.appcompat}"
    const val androidx_browser = "androidx.browser:browser:${FocusVersions.AndroidX.browser}"
    const val androidx_cardview = "androidx.cardview:cardview:${FocusVersions.AndroidX.cardview}"
    const val androidx_compose_ui = "androidx.compose.ui:ui:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_ui_test = "androidx.compose.ui:ui-test-junit4:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_ui_test_manifest = "androidx.compose.ui:ui-test-manifest:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_ui_tooling = "androidx.compose.ui:ui-tooling:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_foundation = "androidx.compose.foundation:foundation:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_material = "androidx.compose.material:material:${FocusVersions.AndroidX.compose}"
    const val androidx_compose_runtime_livedata = "androidx.compose.runtime:runtime-livedata:${FocusVersions.AndroidX.compose}"
    const val androidx_constraint_layout_compose =
        "androidx.constraintlayout:constraintlayout-compose:${FocusVersions.AndroidX.constraint_layout_compose}"
    const val androidx_constraint_layout = "androidx.constraintlayout:constraintlayout:${FocusVersions.AndroidX.constraint_layout}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${FocusVersions.AndroidX.core}"
    const val androidx_fragment = "androidx.fragment:fragment:${FocusVersions.AndroidX.fragment}"

    const val androidx_palette = "androidx.palette:palette-ktx:${FocusVersions.AndroidX.palette}"
    const val androidx_preference = "androidx.preference:preference-ktx:${FocusVersions.AndroidX.preference}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${FocusVersions.AndroidX.recyclerview}"
    const val androidx_lifecycle_process = "androidx.lifecycle:lifecycle-process:${FocusVersions.AndroidX.lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${FocusVersions.AndroidX.lifecycle}"
    const val androidx_splashscreen = "androidx.core:core-splashscreen:${FocusVersions.AndroidX.splashscreen}"
    const val androidx_savedstate = "androidx.savedstate:savedstate-ktx:${FocusVersions.AndroidX.savedstate}"
    const val androidx_transition = "androidx.transition:transition:${FocusVersions.AndroidX.transition}"
    const val androidx_work_ktx = "androidx.work:work-runtime-ktx:${FocusVersions.AndroidX.work}"
    const val androidx_work_testing = "androidx.work:work-testing:${FocusVersions.AndroidX.work}"

    const val google_material = "com.google.android.material:material:${FocusVersions.Google.material}"
    const val google_accompanist_insets_ui = "com.google.accompanist:accompanist-insets-ui:${FocusVersions.Google.accompanist}"
    const val google_play = "com.google.android.play:core:${FocusVersions.Google.play}"
    const val kotlin_gradle_plugin = "org.jetbrains.kotlin:kotlin-gradle-plugin:${FocusVersions.Gradle.kotlin_plugin}"
    const val android_gradle_plugin = "com.android.tools.build:gradle:${FocusVersions.Gradle.android_plugin}"
    const val jna = "net.java.dev.jna:jna:${FocusVersions.ThirdParty.jna}@jar"
    const val leakcanary = "com.squareup.leakcanary:leakcanary-android-core:${FocusVersions.ThirdParty.leakcanary}"
    const val sentry = "io.sentry:sentry-android:${FocusVersions.ThirdParty.sentry}"

    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-core:${FocusVersions.Kotlin.coroutines}"
    const val kotlin_coroutines_android = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${FocusVersions.Kotlin.coroutines}"

    const val adjust = "com.adjust.sdk:adjust-android:${FocusVersions.Adjust.adjust}"
    const val install_referrer = "com.android.installreferrer:installreferrer:${FocusVersions.Adjust.install_referrer}"

    const val androidx_junit_ktx = "androidx.test.ext:junit-ktx:${FocusVersions.Testing.androidx_ext_junit}"
    const val androidx_orchestrator = "androidx.test:orchestrator:${FocusVersions.Testing.androidx_orchestrator}"
    const val androidx_test_core = "androidx.test:core:${FocusVersions.Testing.androidx_core}"
    const val androidx_test_core_ktx = "androidx.test:core-ktx:${FocusVersions.Testing.androidx_core}"
    const val androidx_test_rules = "androidx.test:rules:${FocusVersions.Testing.androidx_core}"
    const val androidx_test_runner = "androidx.test:runner:${FocusVersions.Testing.androidx_core}"
    const val androidx_uiautomator = "androidx.test.uiautomator:uiautomator:${FocusVersions.Testing.androidx_uiautomator}"
    const val espresso_contrib = "androidx.test.espresso:espresso-contrib:${FocusVersions.Testing.androidx_espresso}"
    const val espresso_core = "androidx.test.espresso:espresso-core:${FocusVersions.Testing.androidx_espresso}"
    const val espresso_idling_resource = "androidx.test.espresso:espresso-idling-resource:${FocusVersions.Testing.androidx_espresso}"
    const val espresso_intents = "androidx.test.espresso:espresso-intents:${FocusVersions.Testing.androidx_espresso}"
    const val espresso_web = "androidx.test.espresso:espresso-web:${FocusVersions.Testing.androidx_espresso}"
    const val falcon = "com.jraska:falcon:${FocusVersions.Testing.falcon}"
    const val fastlane = "tools.fastlane:screengrab:${FocusVersions.Testing.fastlane}"
    const val testing_robolectric = "org.robolectric:robolectric:${FocusVersions.Testing.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${FocusVersions.Testing.mockito}"
    const val testing_mockwebserver = "com.squareup.okhttp3:mockwebserver:${FocusVersions.Testing.mockwebserver}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${FocusVersions.Kotlin.coroutines}"

    const val testing_junit_api = "org.junit.jupiter:junit-jupiter-api:${FocusVersions.Testing.junit}"
    const val testing_junit_engine = "org.junit.jupiter:junit-jupiter-engine:${FocusVersions.Testing.junit}"
    const val testing_junit_params = "org.junit.jupiter:junit-jupiter-params:${FocusVersions.Testing.junit}"
}
