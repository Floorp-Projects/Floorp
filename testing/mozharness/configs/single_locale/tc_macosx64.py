import os

tooltool_url = 'http://taskcluster/tooltool.mozilla-releng.net/'
if os.environ.get('TASKCLUSTER_ROOT_URL', 'https://taskcluster.net') != 'https://taskcluster.net':
    # Pre-point tooltool at staging cluster so we can run in parallel with legacy cluster
    tooltool_url = 'http://taskcluster/tooltool.staging.mozilla-releng.net/'

config = {
    "bootstrap_env": {
        "NO_MERCURIAL_SETUP_CHECK": "1",
        "MOZ_OBJDIR": "obj-firefox",
        "EN_US_BINARY_URL": os.environ["EN_US_BINARY_URL"],
        "DIST": "%(abs_objdir)s",
        "L10NBASEDIR": "../../l10n",
        'TOOLTOOL_CACHE': os.environ.get('TOOLTOOL_CACHE'),
    },
    "upload_env": {
        'UPLOAD_PATH': '/builds/worker/artifacts/',
    },

    "tooltool_url": tooltool_url,

    "vcs_share_base": "/builds/hg-shared",
}

