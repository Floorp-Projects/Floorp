config = {
    'perfherder_extra_options': ['static-analysis'],
    'stage_platform': 'win64-st-an-debug',
    'debug_build': True,
    'env': {
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        # Disable sccache because otherwise we won't index the files that
        # sccache optimizes away compilation for
        'SCCACHE_DISABLE': '1',
    },
    'mozconfig_variant': 'debug-searchfox',
    'artifact_flag_build_variant_in_try': None,
}
