config = {
    'base_name': 'Android armv7 api-16+ %(branch)s non-Gradle',
    'stage_platform': 'android-api-16-gradle',
    'build_type': 'api-16-gradle',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-gradle/nightly',
    'multi_locale_config_platform': 'android',
    'artifact_flag_build_variant_in_try': 'api-16-gradle-artifact',
    # Gradle is temporarily non-Gradle, and non-Gradle implies no geckoview.
    'postflight_build_mach_commands': [],
}
