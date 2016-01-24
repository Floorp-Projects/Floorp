config = {
    'base_name': 'Android armv7 API 15+ b2gdroid %(branch)s',
    'stage_platform': 'android-api-15-b2gdroid',
    'build_type': 'api-15-b2gdroid-opt',
    'src_mozconfig': 'mobile/android/b2gdroid/config/mozconfigs/nightly',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/b2gdroid/releng.manifest',
    'multi_locale_config_platform': 'android',
    'enable_nightly_promotion': True,
}
