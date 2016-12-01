config = {
    'base_name': 'Android 4.2 x86 %(branch)s build',
    'stage_platform': 'android-x86',
    'publish_nightly_en_US_routes': False,
    'build_type': 'x86-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-x86/nightly',
    'tooltool_manifest_src': 'mobile/android/config/tooltool-manifests/android-x86/releng.manifest',
    'artifact_flag_build_variant_in_try': 'x86-artifact',
}
