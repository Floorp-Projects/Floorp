# this is a dict of pool specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

config = {
    "taskcluster": {
        # use the relengapi proxy to talk to tooltool
        "tooltool_servers": ['http://taskcluster/tooltool.mozilla-releng.net/'],
        "tooltool_url": 'http://taskcluster/tooltool.mozilla-releng.net/',
        'upload_env': {
            'UPLOAD_PATH': '/builds/worker/artifacts',
        },
    },
}
