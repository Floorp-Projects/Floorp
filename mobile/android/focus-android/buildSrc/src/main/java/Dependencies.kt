/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Versions {
    const val compose_version = "1.0.0"

    object AndroidX {
        const val activity_compose = "1.3.0"
        const val annotation = "1.1.0"
        const val appcompat = "1.3.0"
        const val arch = "2.1.0"
        const val browser = "1.3.0"
        const val core = "1.3.2"
        const val compose = compose_version
        const val cardview = "1.0.0"
        const val recyclerview = "1.2.0"
        const val palette = "1.0.0"
        const val preferences = "1.1.1"
        const val lifecycle = "2.2.0"
    }

    object Google {
        const val material = "1.2.1"
    }

    object Kotlin {
        const val version = "1.5.10"
        const val coroutines = "1.5.0"
    }

    object Gradle {
        const val kotlin_plugin = Kotlin.version
        const val android_plugin = "7.0.0"
    }

    object Test {
        const val robolectric = "4.6.1"
        const val mockito = "3.11.0"
        const val coroutines = "1.5.0"
    }
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
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.AndroidX.core}"
    const val androidx_palette = "androidx.palette:palette-ktx:${Versions.AndroidX.palette}"
    const val androidx_preferences = "androidx.preference:preference-ktx:${Versions.AndroidX.preferences}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.AndroidX.recyclerview}"
    const val androidx_lifecycle_extensions = "androidx.lifecycle:lifecycle-extensions:${Versions.AndroidX.lifecycle}"

    const val google_material = "com.google.android.material:material:${Versions.Google.material}"

    const val kotlin_gradle_plugin = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.Gradle.kotlin_plugin}"
    const val android_gradle_plugin = "com.android.tools.build:gradle:${Versions.Gradle.android_plugin}"

    const val kotlin_stdlib = "org.jetbrains.kotlin:kotlin-stdlib:${Versions.Kotlin.version}"
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-core:${Versions.Kotlin.coroutines}"
    const val kotlin_coroutines_android = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.Kotlin.coroutines}"

    const val testing_robolectric = "org.robolectric:robolectric:${Versions.Test.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${Versions.Test.mockito}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${Versions.Test.coroutines}"
}
