config = {
    "l10n_dir": "l10n-central",
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],

    "objdir": "obj-firefox",
    "repack_env": {
        "MOZ_OBJDIR": "obj-firefox",
    },

    'vcs_share_base': "/builds/hg-shared",

    "mozconfig": "src/mobile/android/config/mozconfigs/android-api-16/l10n-nightly",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/src",
    },

    "upload_env": {
        'UPLOAD_PATH': '/builds/worker/artifacts/',
    },
    'secret_files': [
        {'filename': '/builds/gls-gapi.data',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/gls-gapi.data',
         'min_scm_level': 1},
        {'filename': '/builds/sb-gapi.data',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/sb-gapi.data',
         'min_scm_level': 1},
        {'filename': '/builds/mozilla-fennec-geoloc-api.key',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/mozilla-fennec-geoloc-api.key',
         'min_scm_level': 2, 'default': 'try-build-has-no-secrets'},
        {'filename': '/builds/adjust-sdk.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/adjust-sdk.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/adjust-sdk-sandbox.token'},
        {'filename': '/builds/adjust-sdk-beta.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/adjust-sdk-beta.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/adjust-sdk-sandbox.token'},
        {'filename': '/builds/leanplum-sdk-release.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/leanplum-sdk-release.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/leanplum-sdk-sandbox.token'},
        {'filename': '/builds/leanplum-sdk-beta.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/leanplum-sdk-beta.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/leanplum-sdk-sandbox.token'},
        {'filename': '/builds/leanplum-sdk-nightly.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/leanplum-sdk-nightly.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/leanplum-sdk-sandbox.token'},
        {'filename': '/builds/pocket-api-release.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/pocket-api-release.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/pocket-api-sandbox.token'},
        {'filename': '/builds/pocket-api-beta.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/pocket-api-beta.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/pocket-api-sandbox.token'},
        {'filename': '/builds/pocket-api-nightly.token',
         'secret_name': 'project/releng/gecko/build/level-%(scm-level)s/pocket-api-nightly.token',
         'min_scm_level': 2, 'default-file': '{abs_mozilla_dir}/mobile/android/base/pocket-api-sandbox.token'},

    ],
}
