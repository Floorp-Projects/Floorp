config = {
    'stage_platform': 'android-geckoview-docs',
    'src_mozconfig': 'mobile/android/config/mozconfigs/android-api-16/nightly-android-lints',
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
    'max_build_output_timeout': 0,
}
