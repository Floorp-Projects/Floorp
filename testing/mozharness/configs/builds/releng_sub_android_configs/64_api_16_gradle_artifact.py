config = {
    'base_name': 'Android armv7 api-16+ %(branch)s Gradle Artifact build',
    'stage_platform': 'android-api-16-gradle',
    'build_type': 'api-16-gradle-artifact',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-gradle/nightly-artifact',
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
}
