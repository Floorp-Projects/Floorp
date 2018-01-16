config = {
    'base_name': 'Android GeckoView docs %(branch)s',
    'stage_platform': 'android-geckoview-docs',
    'build_type': 'api-16-opt',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16-frontend/nightly',
    'multi_locale_config_platform': 'android',
    # geckoview-docs doesn't produce a package. So don't collect package metrics.
    'disable_package_metrics': True,
    'postflight_build_mach_commands': [
        ['android',
         'geckoview-docs',
         '--archive',
         '--upload', 'mozilla/geckoview',
         '--upload-branch', 'gh-pages/javadoc/{project}',
         '--upload-message', 'Update {project} javadoc to rev {revision}',
        ],
    ],
    'artifact_flag_build_variant_in_try': None, # There's no artifact equivalent.
    'max_build_output_timeout': 0,
}
