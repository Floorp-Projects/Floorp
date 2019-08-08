/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object Versions {
    const val kotlin = "1.3.30"
    const val coroutines = "1.2.1"
    const val android_gradle_plugin = "3.4.1"
    const val rxAndroid = "2.1.0"
    const val rxKotlin = "2.3.0"
    const val rxBindings = "3.0.0-alpha2"
    const val anko = "0.10.8"
    const val sentry = "1.7.10"
    const val leakcanary = "1.6.3"
    const val leanplum = "5.2.3"
    const val osslicenses_plugin = "0.9.5"
    const val osslicenses_library = "17.0.0"

    const val androidx_appcompat = "1.1.0-rc01"
    const val androidx_coordinator_layout = "1.1.0-beta01"
    const val androidx_constraint_layout = "2.0.0-beta2"
    const val androidx_preference = "1.1.0-rc01"
    const val androidx_legacy = "1.0.0"
    const val androidx_annotation = "1.1.0"
    const val androidx_lifecycle = "2.2.0-alpha03"
    const val androidx_fragment = "1.2.0-alpha02"
    const val androidx_navigation = "2.2.0-alpha01"
    const val androidx_recyclerview = "1.1.0-beta02"
    const val androidx_lifecycle_savedstate = "1.0.0-alpha01"
    const val androidx_testing = "1.2.1-alpha02"
    const val androidx_test_ext = "1.0.0"
    const val androidx_core = "1.2.0-alpha03"
    const val androidx_paging = "2.1.0"
    const val androidx_transition = "1.1.0"
    const val androidx_work = "2.0.1"
    const val google_material = "1.1.0-alpha07"

    const val mozilla_android_components = "8.0.0-SNAPSHOT"
    // Note that android-components also depends on application-services,
    // and in fact is our main source of appservices-related functionality.
    // The version number below tracks the application-services version
    // that we depend on directly for the fenix-megazord (and for it's
    // forUnitTest variant), and it's important that it be kept in
    // sync with the version used by android-components above.
    const val mozilla_appservices = "0.36.0"

    const val autodispose = "1.1.0"
    const val adjust = "4.11.4"
    const val installreferrer = "1.0"

    const val junit = "4.12"
    const val mockito = "2.24.5"
    const val mockk = "1.9.kotlin12"
    const val assertk = "0.19"
    const val flipper = "0.21.0"
    const val soLoader = "0.5.1"

    const val espresso_core = "2.2.2"
    const val espresso_version = "3.0.2"
    const val mockwebserver = "3.11.0"
    const val orchestrator = "1.3.0-alpha02"
    const val tools_test_rules = "1.3.0-alpha02"
    const val tools_test_runner = "1.3.0-alpha02"
    const val uiautomator = "2.1.3"
    const val robolectric = "4.2"

    const val google_ads_id_version = "16.0.0"
}

@Suppress("unused")
object Deps {
    const val tools_androidgradle = "com.android.tools.build:gradle:${Versions.android_gradle_plugin}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.kotlin}"
    const val kotlin_stdlib = "org.jetbrains.kotlin:kotlin-stdlib-jdk7:${Versions.kotlin}"
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-core:${Versions.coroutines}"
    const val kotlin_coroutines_android = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.coroutines}"

    const val allopen = "org.jetbrains.kotlin:kotlin-allopen:${Versions.kotlin}"
    const val osslicenses_plugin = "com.google.android.gms:oss-licenses-plugin:${Versions.osslicenses_plugin}"
    const val osslicenses_library = "com.google.android.gms:play-services-oss-licenses:${Versions.osslicenses_library}"

    const val rxKotlin = "io.reactivex.rxjava2:rxkotlin:${Versions.rxKotlin}"
    const val rxAndroid = "io.reactivex.rxjava2:rxandroid:${Versions.rxAndroid}"
    const val rxBindings = "com.jakewharton.rxbinding3:rxbinding:${Versions.rxBindings}"

    const val anko_commons = "org.jetbrains.anko:anko-commons:${Versions.anko}"
    const val anko_sdk = "org.jetbrains.anko:anko-sdk25:${Versions.anko}"
    const val anko_constraintlayout = "org.jetbrains.anko:anko-constraint-layout:${Versions.anko}"

    const val mozilla_concept_engine = "org.mozilla.components:concept-engine:${Versions.mozilla_android_components}"
    const val mozilla_concept_push = "org.mozilla.components:concept-push:${Versions.mozilla_android_components}"
    const val mozilla_concept_tabstray = "org.mozilla.components:concept-tabstray:${Versions.mozilla_android_components}"
    const val mozilla_concept_toolbar = "org.mozilla.components:concept-toolbar:${Versions.mozilla_android_components}"
    const val mozilla_concept_storage = "org.mozilla.components:concept-storage:${Versions.mozilla_android_components}"
    const val mozilla_concept_sync = "org.mozilla.components:concept-sync:${Versions.mozilla_android_components}"

    const val mozilla_browser_awesomebar = "org.mozilla.components:browser-awesomebar:${Versions.mozilla_android_components}"
    const val mozilla_browser_engine_gecko_nightly = "org.mozilla.components:browser-engine-gecko-nightly:${Versions.mozilla_android_components}"
    const val mozilla_browser_engine_gecko_beta = "org.mozilla.components:browser-engine-gecko-beta:${Versions.mozilla_android_components}"
    const val mozilla_browser_engine_gecko_release = "org.mozilla.components:browser-engine-gecko:${Versions.mozilla_android_components}"
    const val mozilla_browser_domains = "org.mozilla.components:browser-domains:${Versions.mozilla_android_components}"
    const val mozilla_browser_icons = "org.mozilla.components:browser-icons:${Versions.mozilla_android_components}"
    const val mozilla_browser_search = "org.mozilla.components:browser-search:${Versions.mozilla_android_components}"
    const val mozilla_browser_session = "org.mozilla.components:browser-session:${Versions.mozilla_android_components}"
    const val mozilla_browser_tabstray = "org.mozilla.components:browser-tabstray:${Versions.mozilla_android_components}"
    const val mozilla_browser_toolbar = "org.mozilla.components:browser-toolbar:${Versions.mozilla_android_components}"
    const val mozilla_browser_menu = "org.mozilla.components:browser-menu:${Versions.mozilla_android_components}"
    const val mozilla_browser_errorpages = "org.mozilla.components:browser-errorpages:${Versions.mozilla_android_components}"
    const val mozilla_browser_storage_sync = "org.mozilla.components:browser-storage-sync:${Versions.mozilla_android_components}"

    const val mozilla_feature_accounts = "org.mozilla.components:feature-accounts:${Versions.mozilla_android_components}"
    const val mozilla_feature_app_links = "org.mozilla.components:feature-app-links:${Versions.mozilla_android_components}"
    const val mozilla_feature_awesomebar = "org.mozilla.components:feature-awesomebar:${Versions.mozilla_android_components}"
    const val mozilla_feature_contextmenu = "org.mozilla.components:feature-contextmenu:${Versions.mozilla_android_components}"
    const val mozilla_feature_customtabs = "org.mozilla.components:feature-customtabs:${Versions.mozilla_android_components}"
    const val mozilla_feature_intent = "org.mozilla.components:feature-intent:${Versions.mozilla_android_components}"
    const val mozilla_feature_media = "org.mozilla.components:feature-media:${Versions.mozilla_android_components}"
    const val mozilla_feature_qr = "org.mozilla.components:feature-qr:${Versions.mozilla_android_components}"
    const val mozilla_feature_search = "org.mozilla.components:feature-search:${Versions.mozilla_android_components}"
    const val mozilla_feature_session = "org.mozilla.components:feature-session:${Versions.mozilla_android_components}"
    const val mozilla_feature_tabs = "org.mozilla.components:feature-tabs:${Versions.mozilla_android_components}"
    const val mozilla_feature_downloads = "org.mozilla.components:feature-downloads:${Versions.mozilla_android_components}"
    const val mozilla_feature_storage = "org.mozilla.components:feature-storage:${Versions.mozilla_android_components}"
    const val mozilla_feature_prompts = "org.mozilla.components:feature-prompts:${Versions.mozilla_android_components}"
    const val mozilla_feature_push = "org.mozilla.components:feature-push:${Versions.mozilla_android_components}"
    const val mozilla_feature_toolbar = "org.mozilla.components:feature-toolbar:${Versions.mozilla_android_components}"
    const val mozilla_feature_findinpage = "org.mozilla.components:feature-findinpage:${Versions.mozilla_android_components}"
    const val mozilla_feature_site_permissions = "org.mozilla.components:feature-sitepermissions:${Versions.mozilla_android_components}"
    const val mozilla_feature_readerview = "org.mozilla.components:feature-readerview:${Versions.mozilla_android_components}"
    const val mozilla_feature_tab_collections = "org.mozilla.components:feature-tab-collections:${Versions.mozilla_android_components}"

    const val mozilla_service_firefox_accounts = "org.mozilla.components:service-firefox-accounts:${Versions.mozilla_android_components}"
    const val mozilla_service_fretboard = "org.mozilla.components:service-fretboard:${Versions.mozilla_android_components}"
    const val mozilla_service_glean = "org.mozilla.components:service-glean:${Versions.mozilla_android_components}"
    const val mozilla_service_experiments = "org.mozilla.components:service-experiments:${Versions.mozilla_android_components}"

    const val mozilla_ui_colors = "org.mozilla.components:ui-colors:${Versions.mozilla_android_components}"
    const val mozilla_ui_icons = "org.mozilla.components:ui-icons:${Versions.mozilla_android_components}"

    const val mozilla_lib_crash = "org.mozilla.components:lib-crash:${Versions.mozilla_android_components}"
    const val mozilla_lib_fetch_httpurlconnection = "org.mozilla.components:lib-fetch-httpurlconnection:${Versions.mozilla_android_components}"
    const val mozilla_lib_push_firebase = "org.mozilla.components:lib-push-firebase:${Versions.mozilla_android_components}"

    const val mozilla_ui_publicsuffixlist = "org.mozilla.components:lib-publicsuffixlist:${Versions.mozilla_android_components}"

    const val mozilla_support_base = "org.mozilla.components:support-base:${Versions.mozilla_android_components}"
    const val mozilla_support_ktx = "org.mozilla.components:support-ktx:${Versions.mozilla_android_components}"
    const val mozilla_support_rusthttp = "org.mozilla.components:support-rusthttp:${Versions.mozilla_android_components}"
    const val mozilla_support_rustlog = "org.mozilla.components:support-rustlog:${Versions.mozilla_android_components}"
    const val mozilla_support_utils = "org.mozilla.components:support-utils:${Versions.mozilla_android_components}"
    const val mozilla_support_test = "org.mozilla.components:support-test:${Versions.mozilla_android_components}"

    const val sentry = "io.sentry:sentry-android:${Versions.sentry}"
    const val leakcanary = "com.squareup.leakcanary:leakcanary-android:${Versions.leakcanary}"
    const val leakcanary_noop = "com.squareup.leakcanary:leakcanary-android-no-op:${Versions.leakcanary}"

    const val leanplum = "com.leanplum:leanplum-core:${Versions.leanplum}"

    const val androidx_annotation = "androidx.annotation:annotation:${Versions.androidx_annotation}"
    const val androidx_fragment = "androidx.fragment:fragment-ktx:${Versions.androidx_fragment}"
    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.androidx_appcompat}"
    const val androidx_coordinatorlayout = "androidx.coordinatorlayout:coordinatorlayout:${Versions.androidx_coordinator_layout}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${Versions.androidx_constraint_layout}"
    const val androidx_legacy = "androidx.legacy:legacy-support-v4:${Versions.androidx_legacy}"
    const val androidx_lifecycle_extensions = "androidx.lifecycle:lifecycle-extensions:${Versions.androidx_lifecycle}"
    const val androidx_lifecycle_viewmodel = "androidx.lifecycle:lifecycle-viewmodel-ktx:${Versions.androidx_lifecycle}"
    const val androidx_lifecycle_runtime = "androidx.lifecycle:lifecycle-runtime-ktx:${Versions.androidx_lifecycle}"
    const val androidx_lifecycle_viewmodel_ss = "androidx.lifecycle:lifecycle-viewmodel-savedstate:${Versions.androidx_lifecycle_savedstate}"
    const val androidx_paging = "androidx.paging:paging-runtime-ktx:${Versions.androidx_paging}"
    const val androidx_preference = "androidx.preference:preference-ktx:${Versions.androidx_preference}"
    const val androidx_safeargs = "androidx.navigation:navigation-safe-args-gradle-plugin:${Versions.androidx_navigation}"
    const val androidx_navigation_fragment = "androidx.navigation:navigation-fragment-ktx:${Versions.androidx_navigation}"
    const val androidx_navigation_ui = "androidx.navigation:navigation-ui:${Versions.androidx_navigation}"
    const val androidx_recyclerview = "androidx.recyclerview:recyclerview:${Versions.androidx_recyclerview}"
    const val androidx_core = "androidx.core:core:${Versions.androidx_core}"
    const val androidx_core_ktx = "androidx.core:core-ktx:${Versions.androidx_core}"
    const val androidx_transition = "androidx.transition:transition:${Versions.androidx_transition}"
    const val androidx_work_ktx = "androidx.work:work-runtime-ktx:${Versions.androidx_work}"
    const val google_material = "com.google.android.material:material:${Versions.google_material}"

    const val autodispose = "com.uber.autodispose:autodispose:${Versions.autodispose}"
    const val autodispose_android = "com.uber.autodispose:autodispose-android:${Versions.autodispose}"
    const val autodispose_android_aac = "com.uber.autodispose:autodispose-android-archcomponents:${Versions.autodispose}"
    const val autodispose_android_aac_test = "com.uber.autodispose:autodispose-android-archcomponents-test:${Versions.autodispose}"

    const val adjust = "com.adjust.sdk:adjust-android:${Versions.adjust}"
    const val installreferrer = "com.android.installreferrer:installreferrer:${Versions.installreferrer}"

    const val junit = "junit:junit:${Versions.junit}"
    const val mockito_core = "org.mockito:mockito-core:${Versions.mockito}"
    const val mockito_android = "org.mockito:mockito-android:${Versions.mockito}"
    const val mockk = "io.mockk:mockk:${Versions.mockk}"
    const val assertk = "com.willowtreeapps.assertk:assertk-jvm:${Versions.assertk}"

    const val flipper = "com.facebook.flipper:flipper:${Versions.flipper}"
    const val flipper_noop = "com.facebook.flipper:flipper-noop:${Versions.flipper}"
    const val soLoader = "com.facebook.soloader:soloader:${Versions.soLoader}"

    const val espresso_contrib = "com.android.support.test.espresso:espresso-contrib:${Versions.espresso_version}"
    const val espresso_core = "com.android.support.test.espresso:espresso-core:${Versions.espresso_core}"
    const val espresso_idling_resources = "com.android.support.test.espresso:espresso-idling-resource:${Versions.espresso_version}"
    const val mockwebserver = "com.squareup.okhttp3:mockwebserver:${Versions.mockwebserver}"
    const val orchestrator = "androidx.test:orchestrator:${Versions.orchestrator}"
    const val tools_test_rules = "androidx.test:rules:${Versions.tools_test_rules}"
    const val tools_test_runner = "androidx.test:runner:${Versions.tools_test_runner}"
    const val uiautomator = "com.android.support.test.uiautomator:uiautomator-v18:${Versions.uiautomator}"
    const val robolectric = "org.robolectric:robolectric:${Versions.robolectric}"
    const val fragment_testing = "androidx.fragment:fragment-testing:${Versions.androidx_fragment}"
    const val androidx_junit = "androidx.test.ext:junit:${Versions.androidx_test_ext}"
    const val androidx_test_core ="androidx.test:core:${Versions.androidx_testing}"

    const val fenix_megazord = "org.mozilla.appservices:fenix-megazord:${Versions.mozilla_appservices}"
    const val fenix_megazord_forUnitTests = "org.mozilla.appservices:fenix-megazord-forUnitTests:${Versions.mozilla_appservices}"

    const val google_ads_id = "com.google.android.gms:play-services-ads-identifier:${Versions.google_ads_id_version}"
}
