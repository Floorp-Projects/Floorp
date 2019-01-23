/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

private object Versions {
    const val kotlin = "1.3.11"
    const val android_gradle_plugin = "3.2.1"
    const val geckoNightly = "66.0.20190111093148"

    const val androidx_appcompat = "1.0.2"
    const val androidx_constraint_layout = "2.0.0-alpha3"

    const val mozilla_android_components = "0.40.0-SNAPSHOT"

    const val junit = "4.12"
    const val test_tools = "1.0.2"
    const val espresso_core = "2.2.2"

    const val androidx_legacy = "1.0.0"
    const val android_arch_navigation = "1.0.0-alpha09"
}

@Suppress("unused")
object Deps {
    const val tools_androidgradle = "com.android.tools.build:gradle:${Versions.android_gradle_plugin}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.kotlin}"
    const val kotlin_stdlib = "org.jetbrains.kotlin:kotlin-stdlib-jdk7:${Versions.kotlin}"

    const val geckoview_nightly_arm = "org.mozilla.geckoview:geckoview-nightly-armeabi-v7a:${Versions.geckoNightly}"
    const val geckoview_nightly_x86 = "org.mozilla.geckoview:geckoview-nightly-x86:${Versions.geckoNightly}"

    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.androidx_appcompat}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${Versions.androidx_constraint_layout}"

    const val mozilla_concept_engine = "org.mozilla.components:concept-engine:${Versions.mozilla_android_components}"
    const val mozilla_concept_storage = "org.mozilla.components:concept-storage:${Versions.mozilla_android_components}"

    const val mozilla_browser_awesomebar = "org.mozilla.components:browser-awesomebar:${Versions.mozilla_android_components}"
    const val mozilla_browser_domains = "org.mozilla.components:browser-domains:${Versions.mozilla_android_components}"
    const val mozilla_browser_engine_gecko_nightly = "org.mozilla.components:browser-engine-gecko-nightly:${Versions.mozilla_android_components}"
    const val mozilla_browser_session = "org.mozilla.components:browser-session:${Versions.mozilla_android_components}"
    const val mozilla_browser_toolbar = "org.mozilla.components:browser-toolbar:${Versions.mozilla_android_components}"

    const val mozilla_feature_intent = "org.mozilla.components:feature-intent:${Versions.mozilla_android_components}"
    const val mozilla_feature_session = "org.mozilla.components:feature-session:${Versions.mozilla_android_components}"
    const val mozilla_feature_storage = "org.mozilla.components:feature-storage:${Versions.mozilla_android_components}"
    const val mozilla_feature_toolbar = "org.mozilla.components:feature-toolbar:${Versions.mozilla_android_components}"

    const val mozilla_support_ktx = "org.mozilla.components:support-ktx:${Versions.mozilla_android_components}"

    const val junit = "junit:junit:${Versions.junit}"
    const val tools_test_runner = "com.android.support.test:runner:${Versions.test_tools}"
    const val tools_espresso_core = "com.android.support.test.espresso:espresso-core:${Versions.espresso_core}"

    const val androidx_legacy = "androidx.legacy:legacy-support-v4:${Versions.androidx_legacy}"
    const val android_arch_navigation = "android.arch.navigation:navigation-fragment:${Versions.android_arch_navigation}"
}

