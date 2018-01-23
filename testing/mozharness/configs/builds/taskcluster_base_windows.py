import os

config = {
    'default_actions': [
        'build',
        'check-test',
    ],
    'app_ini_path': '%(obj_dir)s/dist/bin/application.ini',
    # decides whether we want to use moz_sign_cmd in env
    'enable_signing': True,
    'vcs_share_base': os.path.join('y:', os.sep, 'hg-shared'),
    'enable_count_ctors': False,
    'max_build_output_timeout': 60 * 80,

    'publish_nightly_en_US_routes': True,
    'env': {
        'HG_SHARE_BASE_DIR': os.path.join('y:', os.sep, 'hg-shared'),
        'MOZBUILD_STATE_PATH': os.path.join(os.getcwd(), '.mozbuild'),
        'MOZ_CRASHREPORTER_NO_REPORT': '1',
        'MOZ_OBJDIR': '%(abs_obj_dir)s',
        'TINDERBOX_OUTPUT': '1',
        'TOOLTOOL_CACHE': 'c:/builds/tooltool_cache',
        'TOOLTOOL_HOME': '/c/builds',
        'MSYSTEM': 'MINGW32',
    },
    'upload_env': {
        'UPLOAD_HOST': 'localhost',
        'UPLOAD_PATH': os.path.join(os.getcwd(), 'public', 'build'),
    },
}
