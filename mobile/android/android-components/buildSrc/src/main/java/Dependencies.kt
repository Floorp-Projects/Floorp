/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Synchronized version numbers for dependencies used by (some) modules
object Versions {
    const val kotlin = "1.3.21"
    const val coroutines = "1.1.1"

    const val androidx_test = "1.1.0"
    const val androidx_runner = "1.1.0"
    const val androidx_espresso = "3.1.1"

    const val junit = "4.12"
    const val robolectric = "4.1"
    const val mockito = "2.24.5"

    const val mockwebserver = "3.10.0"

    const val support_libraries = "28.0.0"
    const val constraint_layout = "1.1.2"
    const val workmanager = "1.0.0"
    const val lifecycle = "1.1.1"

    const val dokka = "0.9.17"
    const val android_gradle_plugin = "3.2.1"
    const val android_maven_publish_plugin = "3.6.2"
    const val lint = "26.3.2"

    const val sentry = "1.7.21"
    const val okhttp = "3.13.1"

    const val room = "1.1.1"
    const val paging = "1.0.1"
    const val zxing = "3.3.0"

    const val mozilla_appservices = "0.22.0"
    const val servo = "0.0.1.20181017.aa95911"
}

// Synchronized dependencies used by (some) modules
@Suppress("MaxLineLength")
object Dependencies {
    const val kotlin_stdlib = "org.jetbrains.kotlin:kotlin-stdlib-jdk7:${Versions.kotlin}"
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.coroutines}"
    const val kotlin_reflect = "org.jetbrains.kotlin:kotlin-reflect:${Versions.kotlin}"

    const val testing_junit = "junit:junit:${Versions.junit}"
    const val testing_robolectric = "org.robolectric:robolectric:${Versions.robolectric}"
    const val testing_mockito = "org.mockito:mockito-core:${Versions.mockito}"
    const val testing_mockwebserver = "com.squareup.okhttp3:mockwebserver:${Versions.mockwebserver}"
    const val testing_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-test:${Versions.coroutines}"

    const val androidx_test_core = "androidx.test:core-ktx:${Versions.androidx_test}"
    const val androidx_test_runner = "androidx.test:runner:${Versions.androidx_test}"
    const val androidx_test_rules = "androidx.test:rules:${Versions.androidx_test}"
    const val androidx_work_testing = "android.arch.work:work-testing:${Versions.workmanager}"
    const val androidx_espresso_core = "androidx.test.espresso:espresso-core:${Versions.androidx_espresso}"

    const val support_annotations = "com.android.support:support-annotations:${Versions.support_libraries}"
    const val support_cardview = "com.android.support:cardview-v7:${Versions.support_libraries}"
    const val support_design = "com.android.support:design:${Versions.support_libraries}"
    const val support_recyclerview = "com.android.support:recyclerview-v7:${Versions.support_libraries}"
    const val support_appcompat = "com.android.support:appcompat-v7:${Versions.support_libraries}"
    const val support_customtabs = "com.android.support:customtabs:${Versions.support_libraries}"
    const val support_fragment = "com.android.support:support-fragment:${Versions.support_libraries}"
    const val support_constraintlayout = "com.android.support.constraint:constraint-layout:${Versions.constraint_layout}"
    const val support_compat = "com.android.support:support-compat:${Versions.support_libraries}"

    const val arch_lifecycle = "android.arch.lifecycle:extensions:${Versions.lifecycle}"
    const val arch_lifecycl_compiler = "android.arch.lifecycle:compiler:${Versions.lifecycle}"
    const val arch_workmanager = "android.arch.work:work-runtime-ktx:${Versions.workmanager}"
    const val arch_paging = "android.arch.paging:runtime:${Versions.paging}"

    const val room_runtime = "android.arch.persistence.room:runtime:${Versions.room}"
    const val room_compiler = "android.arch.persistence.room:compiler:${Versions.room}"
    const val room_testing = "android.arch.persistence.room:testing:${Versions.room}"

    const val tools_dokka = "org.jetbrains.dokka:dokka-android-gradle-plugin:${Versions.dokka}"
    const val tools_androidgradle = "com.android.tools.build:gradle:${Versions.android_gradle_plugin}"
    const val tools_kotlingradle = "org.jetbrains.kotlin:kotlin-gradle-plugin:${Versions.kotlin}"
    const val tools_androidmavenpublish = "digital.wup:android-maven-publish:${Versions.android_maven_publish_plugin}"

    const val tools_lint = "com.android.tools.lint:lint:${Versions.lint}"
    const val tools_lintapi = "com.android.tools.lint:lint-api:${Versions.lint}"
    const val tools_linttests = "com.android.tools.lint:lint-tests:${Versions.lint}"

    const val mozilla_fxa = "org.mozilla.appservices:fxaclient:${Versions.mozilla_appservices}"
    const val mozilla_sync_logins = "org.mozilla.appservices:logins:${Versions.mozilla_appservices}"
    const val mozilla_places = "org.mozilla.appservices:places:${Versions.mozilla_appservices}"
    const val mozilla_rustlog = "org.mozilla.appservices:rustlog:${Versions.mozilla_appservices}"
    const val mozilla_servo_arm = "org.mozilla.servoview:servoview-armv7:${Versions.servo}"
    const val mozilla_servo_x86 = "org.mozilla.servoview:servoview-x86:${Versions.servo}"

    const val thirdparty_okhttp = "com.squareup.okhttp3:okhttp:${Versions.okhttp}"
    const val thirdparty_okhttp_urlconnection = "com.squareup.okhttp3:okhttp-urlconnection:${Versions.okhttp}"
    const val thirdparty_sentry = "io.sentry:sentry-android:${Versions.sentry}"
    const val thirdparty_zxing = "com.google.zxing:core:${Versions.zxing}"
}
