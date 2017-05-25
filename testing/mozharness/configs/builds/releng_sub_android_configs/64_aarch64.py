config = {
    'base_name': 'Android aarch64 API 21+ %(branch)s',
    'stage_platform': 'android-aarch64',
    'build_type': 'aarch64-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-aarch64/nightly',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android/releng.manifest',
    'multi_locale_config_platform': 'android',
    'artifact_flag_build_variant_in_try': 'aarch64-artifact',
}
