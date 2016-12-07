config = {
    'base_name': 'Android armv7 API 15+ %(branch)s debug',
    'stage_platform': 'android-api-15-debug',
    'build_type': 'api-15-debug',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-15/debug',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android/releng.manifest',
    'multi_locale_config_platform': 'android',
    'debug_build': True,
    'artifact_flag_build_variant_in_try': 'api-15-debug-artifact',
}
