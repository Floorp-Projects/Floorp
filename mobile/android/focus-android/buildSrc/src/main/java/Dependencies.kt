/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Versions {
    object Adjust {
        const val adjust = "4.33.0"
        const val install_referrer = "2.2"
    }

    object AndroidX {
        const val annotation = "1.5.0"
        const val appcompat = "1.3.0"
        const val arch = "2.1.0"
        const val browser = "1.3.0"
        const val cardview = "1.0.0"
        const val compose = "1.3.0"
        const val constraint_layout = "2.1.4"
        const val constraint_layout_compose = "1.0.1"
        const val core = "1.9.0"
        const val fragment = "1.5.2"
        const val lifecycle = "2.5.1"
        const val palette = "1.0.0"
        const val preference = "1.1.1"
        const val recyclerview = "1.2.0"
        const val savedstate = "1.2.0"
        const val splashscreen = "1.0.0"
        const val transition = "1.4.0"
        const val work = "2.7.1"
    }

    object Google {
        const val accompanist = "0.16.1"
        const val compose_compiler = "1.3.2"
        const val material = "1.2.1"
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
        const val leakcanary = "2.9.1"
        const val sentry = "6.6.0"
    }

    // Workaround for a Gradle parsing bug that prevents using nested objects directly in Gradle files.
    // These might be removable if we switch to kts files instead.
    // https://github.com/gradle/gradle/issues/9251
    const val google_compose_compiler = Versions.Google.compose_compiler
    const val ktlint_version = Versions.Testing.ktlint
}

object Dependencies {
    const val androidx_annotation = "androidx.annotation:annotation:${Versions.AndroidX.annotation}"
    const val androidx_arch_core_testing = "androidx.arch.core:core-testing:${Versions.AndroidX.arch}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.AndroidX.appcompat}"
    const val androidx_browser = "androidx.browser:browser:${Versions.AndroidX.browser}"
    const val androidx_cardview = "androidx.cardview:cardview:${Versions.AndroidX.cardview}"
    const val androidx_compose_ui = "androidx.compose.ui:ui:${Versions.AndroidX.compose}"
    const val androidx_compose_ui_test = "androidx.compose.ui:ui-test-junit4:${Versions.AndroidX.compose}"
    const val androidx_compose_ui_test_manifest = "androidx.compose.ui:ui-test-manifest:${Versions.AndroidX.compose}"
    const val androidx_compose_ui_tooling = "androidx.compose.ui:ui-tooling:${Versions.AndroidX.compose}"
    const val androidx_compose_foundation = "androidx.compose.foundation:foundation:${Versions.AndroidX.compose}"
    const val androidx_compose_material = "androidx.compose.material:material:${Versions.AndroidX.compose}"
    const val androidx_compose_runtime_livedata = "androidx.compose.runtime:runtime-livedata:${Versions.AndroidX.compose}"
    const val androidx_constraint_layout_compose =
        "androidx.constraintlayout:constraintlayout-compose:${Versions.AndroidX.constraint_layout_compose}"
    const val androidx_constraint_layout = "androidx.constraintlayout:constraintlayout:${Versions.AndroidX.constraint_layout}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.AndroidX.core}"
    const val androidx_fragment = "androidx.fragment:fragment:${Versions.AndroidX.fragment}"

    const val androidx_palette = "androidx.palette:palette-ktx:${Versions.AndroidX.palette}"
    const val androidx_preference = "androidx.preference:preference-ktx:${Versions.AndroidX.preference}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.AndroidX.recyclerview}"
    const val androidx_lifecycle_process = "androidx.lifecycle:lifecycle-process:${Versions.AndroidX.lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${Versions.AndroidX.lifecycle}"
    const val androidx_splashscreen = "androidx.core:core-splashscreen:${Versions.AndroidX.splashscreen}"
    const val androidx_savedstate = "androidx.savedstate:savedstate-ktx:${Versions.AndroidX.savedstate}"
    const val androidx_transition = "androidx.transition:transition:${Versions.AndroidX.transition}"
    const val androidx_work_ktx = "androidx.work:work-runtime-ktx:${Versions.AndroidX.work}"
    const val androidx_work_testing = "androidx.work:work-testing:${Versions.AndroidX.work}"

    const val google_material = "com.google.android.material:material:${Versions.Google.material}"
    const val google_accompanist_insets_ui = "com.google.accompanist:accompanist-insets-ui:${Versions.Google.accompanist}"
    const val google_play = "com.google.android.play:core:${Versions.Google.play}"
    const val kotlin_gradle_plugin = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.Gradle.kotlin_plugin}"
    const val android_gradle_plugin = "com.android.tools.build:gradle:${Versions.Gradle.android_plugin}"
    const val jna = "net.java.dev.jna:jna:${Versions.ThirdParty.jna}@jar"
    const val leakcanary = "com.squareup.leakcanary:leakcanary-android-core:${Versions.ThirdParty.leakcanary}"
    const val sentry = "io.sentry:sentry-android:${Versions.ThirdParty.sentry}"

    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-core:${Versions.Kotlin.coroutines}"
    const val kotlin_coroutines_android = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.Kotlin.coroutines}"

    const val adjust = "com.adjust.sdk:adjust-android:${Versions.Adjust.adjust}"
    const val install_referrer = "com.android.installreferrer:installreferrer:${Versions.Adjust.install_referrer}"

    const val androidx_junit_ktx = "androidx.test.ext:junit-ktx:${Versions.Testing.androidx_ext_junit}"
    const val androidx_orchestrator = "androidx.test:orchestrator:${Versions.Testing.androidx_orchestrator}"
    const val androidx_test_core = "androidx.test:core:${Versions.Testing.androidx_core}"
    const val androidx_test_core_ktx = "androidx.test:core-ktx:${Versions.Testing.androidx_core}"
    const val androidx_test_rules = "androidx.test:rules:${Versions.Testing.androidx_core}"
    const val androidx_test_runner = "androidx.test:runner:${Versions.Testing.androidx_core}"
    const val androidx_uiautomator = "androidx.test.uiautomator:uiautomator:${Versions.Testing.androidx_uiautomator}"
    const val espresso_contrib = "androidx.test.espresso:espresso-contrib:${Versions.Testing.androidx_espresso}"
    const val espresso_core = "androidx.test.espresso:espresso-core:${Versions.Testing.androidx_espresso}"
    const val espresso_idling_resource = "androidx.test.espresso:espresso-idling-resource:${Versions.Testing.androidx_espresso}"
    const val espresso_intents = "androidx.test.espresso:espresso-intents:${Versions.Testing.androidx_espresso}"
    const val espresso_web = "androidx.test.espresso:espresso-web:${Versions.Testing.androidx_espresso}"
    const val falcon = "com.jraska:falcon:${Versions.Testing.falcon}"
    const val fastlane = "tools.fastlane:screengrab:${Versions.Testing.fastlane}"
    const val testing_robolectric = "org.robolectric:robolectric:${Versions.Testing.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${Versions.Testing.mockito}"
    const val testing_mockwebserver = "com.squareup.okhttp3:mockwebserver:${Versions.Testing.mockwebserver}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${Versions.Kotlin.coroutines}"

    const val testing_junit_api = "org.junit.jupiter:junit-jupiter-api:${Versions.Testing.junit}"
    const val testing_junit_engine = "org.junit.jupiter:junit-jupiter-engine:${Versions.Testing.junit}"
    const val testing_junit_params = "org.junit.jupiter:junit-jupiter-params:${Versions.Testing.junit}"
}
