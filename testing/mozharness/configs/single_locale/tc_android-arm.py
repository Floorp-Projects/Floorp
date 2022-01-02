# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

config = {
    "locales_dir": "mobile/android/locales",
    "ignore_locales": ["en-US"],
    "repack_env": {
        "MOZ_OBJDIR": "%(abs_obj_dir)s",
    },
    "vcs_share_base": "/builds/hg-shared",
    "mozconfig": "src/mobile/android/config/mozconfigs/android-arm/l10n-nightly",
    "tooltool_config": {
        "manifest": "mobile/android/config/tooltool-manifests/android/releng.manifest",
        "output_dir": "%(abs_work_dir)s/src",
    },
    "secret_files": [
        {
            "filename": "/builds/gls-gapi.data",
            "secret_name": "project/releng/gecko/build/level-%(scm-level)s/gls-gapi.data",
            "min_scm_level": 1,
        },
        {
            "filename": "/builds/sb-gapi.data",
            "secret_name": "project/releng/gecko/build/level-%(scm-level)s/sb-gapi.data",
            "min_scm_level": 1,
        },
        {
            "filename": "/builds/mozilla-fennec-geoloc-api.key",
            "secret_name": "project/releng/gecko/build/level-%(scm-level)s/mozilla-fennec-geoloc-api.key",
            "min_scm_level": 2,
            "default": "try-build-has-no-secrets",
        },
    ],
}
