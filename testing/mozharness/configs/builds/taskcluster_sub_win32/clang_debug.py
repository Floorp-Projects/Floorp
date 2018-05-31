config = {
    'stage_platform': 'win32-st-an-debug',
    'debug_build': True,
    'env': {
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
    },
    'mozconfig_variant': 'clang-debug',
    'artifact_flag_build_variant_in_try': None,
}
