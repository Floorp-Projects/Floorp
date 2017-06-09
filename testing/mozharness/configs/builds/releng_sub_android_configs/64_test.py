config = {
    'base_name': 'Android armv7 unit tests %(branch)s',
    'stage_platform': 'android-test',
    'build_type': 'api-15-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-15-frontend/nightly',
    'multi_locale_config_platform': 'android',
    # unit tests don't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['gradle',
         'app:testOfficialAustralisDebugUnitTest',
         'app:testOfficialPhotonDebugUnitTest',
        ],
    ],
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
}
