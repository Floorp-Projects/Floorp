import os

platform = "win64"

tooltool_url = 'http://taskcluster/tooltool.mozilla-releng.net/'
if os.environ.get('TASKCLUSTER_ROOT_URL', 'https://taskcluster.net') != 'https://taskcluster.net':
    # Pre-point tooltool at staging cluster so we can run in parallel with legacy cluster
    tooltool_url = 'http://taskcluster/tooltool.staging.mozilla-releng.net/'

config = {
    "repack_id": os.environ.get("REPACK_ID"),

    'tooltool_url': tooltool_url,
    'run_configure': False,

    'env': {
        'PATH': "%(abs_input_dir)s/upx/bin:%(PATH)s",
    }
}
