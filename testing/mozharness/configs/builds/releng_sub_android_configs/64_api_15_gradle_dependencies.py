config = {
    'base_name': 'Android armv7 API 15+ Gradle dependencies %(branch)s',
    'stage_platform': 'android-api-15-gradle-dependencies',
    'build_type': 'api-15-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-15-gradle-dependencies/nightly',
    'multi_locale_config_platform': 'android',
     # gradle-dependencies doesn't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'gradle-dependencies',
        ],
    ],
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
}
