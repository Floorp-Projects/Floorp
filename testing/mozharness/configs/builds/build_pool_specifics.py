# this is a dict of pool specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently
import os

tooltool_url = 'http://taskcluster/tooltool.mozilla-releng.net/'
if os.environ.get('TASKCLUSTER_ROOT_URL', 'https://taskcluster.net') != 'https://taskcluster.net':
    # Pre-point tooltool at staging cluster so we can run in parallel with legacy cluster
    tooltool_url = 'http://taskcluster/tooltool.staging.mozilla-releng.net/'

config = {
    "taskcluster": {
        # use the relengapi proxy to talk to tooltool
        "tooltool_servers": [tooltool_url],
        "tooltool_url": tooltool_url,
        'upload_env': {
            'UPLOAD_PATH': '/builds/worker/artifacts',
        },
    },
}
