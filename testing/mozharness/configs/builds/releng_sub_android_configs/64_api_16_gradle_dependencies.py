config = {
    'base_name': 'Android armv7 api-16+ Gradle dependencies %(branch)s',
    'stage_platform': 'android-api-16-gradle-dependencies',
    'build_type': 'api-16-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-gradle-dependencies/nightly',
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
