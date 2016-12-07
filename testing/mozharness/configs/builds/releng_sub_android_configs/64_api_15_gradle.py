config = {
    'base_name': 'Android armv7 API 15+ %(branch)s Gradle',
    'stage_platform': 'android-api-15-gradle',
    'build_type': 'api-15-gradle',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-15-gradle/nightly',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android/releng.manifest',
    'multi_locale_config_platform': 'android',
    # It's not obvious, but postflight_build is after packaging, so the Gecko
    # binaries are in the object directory, ready to be packaged into the
    # GeckoView AAR.
    'postflight_build_mach_commands': [
        ['gradle',
         'geckoview:assembleWithGeckoBinaries',
         'geckoview_example:assembleWithGeckoBinaries',
         'uploadArchives',
        ],
    ],
    'artifact_flag_build_variant_in_try': 'api-15-gradle-artifact',
}
