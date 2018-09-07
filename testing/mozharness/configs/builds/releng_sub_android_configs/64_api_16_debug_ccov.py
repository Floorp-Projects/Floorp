config = {
    'base_name': 'Android armv7 api-16+ %(branch)s debug coverage',
    'stage_platform': 'android-api-16-debug-ccov',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16/debug-ccov',
    'multi_locale_config_platform': 'android',
    'debug_build': True,
    'postflight_build_mach_commands': [
        ['android',
         'archive-geckoview',
        ],
        ['android',
         'archive-coverage-artifacts',
        ],
    ],
}
