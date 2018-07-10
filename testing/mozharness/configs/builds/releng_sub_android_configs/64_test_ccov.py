config = {
    'base_name': 'Android armv7 unit test code coverage %(branch)s',
    'stage_platform': 'android-test-ccov',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-frontend/nightly',
    'multi_locale_config_platform': 'android',
    # unit tests don't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'test-ccov',
        ],
    ],
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
    'max_build_output_timeout': 0,
}
