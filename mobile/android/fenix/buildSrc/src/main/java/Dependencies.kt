object Versions {
    const val kotlin = "1.3.10"
    const val coroutines = "1.0.1"
    const val geckoNightly = "66.0.20181217093726"

    const val androidx_appcompat = "1.0.2"
    const val androidx_constraintlayout = "1.1.3"

    const val junit = "4.12"
    const val test_tools = "1.0.2"
    const val espresso_core = "2.2.2"
}

object Deps {
    const val kotlin_stdlib = "org.jetbrains.kotlin:kotlin-stdlib-jdk7:${Versions.kotlin}"
    const val kotlin_coroutines = "org.jetbrains.kotlinx:kotlinx-coroutines-android:${Versions.coroutines}"

    const val geckoview_nightly_arm = "org.mozilla.geckoview:geckoview-nightly-armeabi-v7a:${Versions.geckoNightly}"
    const val geckoview_nightly_x86 = "org.mozilla.geckoview:geckoview-nightly-x86:${Versions.geckoNightly}"

    const val androidx_appcompat = "androidx.appcompat:appcompat:${Versions.androidx_appcompat}"
    const val androidx_constraintlayout = "androidx.constraintlayout:constraintlayout:${Versions.androidx_constraintlayout}"

    const val junit = "junit:junit:${Versions.junit}"
    const val tools_test_runner = "com.android.support.test:runner:${Versions.test_tools}"
    const val tools_espresso_core = "com.android.support.test.espresso:espresso-core:${Versions.espresso_core}"
}