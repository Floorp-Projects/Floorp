config = {
    'stage_platform': 'android-findbugs',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-frontend/nightly',
    'multi_locale_config_platform': 'android',
    # findbugs doesn't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'findbugs',
        ],
    ],
    'max_build_output_timeout': 0,
}
