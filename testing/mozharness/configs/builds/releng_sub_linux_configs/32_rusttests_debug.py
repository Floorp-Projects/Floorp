import os

config = {
    'debug_build': True,
    'stage_platform': 'linux-rusttests-debug',
    #### 32 bit build specific #####
    'env': {
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'DISPLAY': ':2',
        'HG_SHARE_BASE_DIR': '/builds/hg-shared',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'LC_ALL': 'C',
        # 32 bit specific
        'PATH': '/usr/local/bin:/bin:\
/usr/bin:/usr/local/sbin:/usr/sbin:/sbin',
        'LD_LIBRARY_PATH': '%(abs_obj_dir)s/dist/bin',
        'XPCOM_DEBUG_BREAK': 'stack-and-abort',
        'TINDERBOX_OUTPUT': '1',
    },
    'build_targets': ['pre-export', 'export', 'recurse_rusttests'],
    'artifact_flag_build_variant_in_try': None,
    'disable_package_metrics': True,
    #######################
}
