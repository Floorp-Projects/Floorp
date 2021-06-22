/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Versions {
    object AndroidX {
        const val annotation = "1.1.0"
        const val appcompat = "1.3.0"
        const val arch = "2.1.0"
        const val browser = "1.3.0"
        const val core = "1.3.2"
        const val cardview = "1.0.0"
        const val recyclerview = "1.2.0"
        const val palette = "1.0.0"
        const val preferences = "1.1.1"
        const val lifecycle = "2.2.0"
    }

    object Google {
        const val material = "1.2.1"
    }
}

object Dependencies {
    const val androidx_annotation = "androidx.annotation:annotation:${Versions.AndroidX.annotation}"
    const val androidx_arch_core_testing = "androidx.arch.core:core-testing:${Versions.AndroidX.arch}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.AndroidX.appcompat}"
    const val androidx_browser = "androidx.browser:browser:${Versions.AndroidX.browser}"
    const val androidx_cardview = "androidx.cardview:cardview:${Versions.AndroidX.cardview}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.AndroidX.core}"
    const val androidx_palette = "androidx.palette:palette-ktx:${Versions.AndroidX.palette}"
    const val androidx_preferences = "androidx.preference:preference-ktx:${Versions.AndroidX.preferences}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.AndroidX.recyclerview}"
    const val androidx_lifecycle_extensions = "androidx.lifecycle:lifecycle-extensions:${Versions.AndroidX.lifecycle}"

    const val google_material = "com.google.android.material:material:${Versions.Google.material}"
}
