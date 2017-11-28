config = {
    'stage_platform': 'win64-ccov',
    'debug_build': True,
    'env': {
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
    },
    'mozconfig_variant': 'code-coverage',
    'artifact_flag_build_variant_in_try': None,
}
