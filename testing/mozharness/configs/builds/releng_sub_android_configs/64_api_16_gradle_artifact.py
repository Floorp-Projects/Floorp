config = {
    'base_name': 'Android armv7 api-16+ %(branch)s non-Gradle Artifact build',
    'stage_platform': 'android-api-16-gradle',
    'build_type': 'api-16-gradle-artifact',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-gradle/nightly-artifact',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android/releng.manifest',
    'multi_locale_config_platform': 'android',
    # Gradle is temporarily non-Gradle, and non-Gradle implies no geckoview.
    'postflight_build_mach_commands': [],
}
