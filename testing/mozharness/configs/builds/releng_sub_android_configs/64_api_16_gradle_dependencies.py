config = {
    'stage_platform': 'android-api-16-gradle-dependencies',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-gradle-dependencies/nightly',
    'multi_locale_config_platform': 'android',
     # gradle-dependencies doesn't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'gradle-dependencies',
        ],
    ],
    'max_build_output_timeout': 0,
}
