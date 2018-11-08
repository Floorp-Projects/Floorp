config = {
    'default_actions': [
        'build',
    ],
    'stage_platform': 'win64-rusttests',
    'env': {
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
    },
    'build_targets': ['pre-export', 'export', 'recurse_rusttests'],
    'disable_package_metrics': True,
    'artifact_flag_build_variant_in_try': None,
}
