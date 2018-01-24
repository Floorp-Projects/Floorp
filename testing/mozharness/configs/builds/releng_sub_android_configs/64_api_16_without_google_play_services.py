config = {
    'base_name': 'Android armv7 api-16+ %(branch)s --without-google-play-services',
    'stage_platform': 'android-api-16',
    'build_type': 'api-16-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16/nightly-without-google-play-services',
    'multi_locale_config_platform': 'android',
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
}
