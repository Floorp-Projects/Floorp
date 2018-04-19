# this is a dict of pool specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

config = {
    "taskcluster": {
        # use the relengapi proxy to talk to tooltool
        "tooltool_servers": ['http://relengapi/tooltool/'],
        "tooltool_url": 'http://relengapi/tooltool/',
        'upload_env': {
            'UPLOAD_PATH': '/builds/worker/artifacts',
        },
    },
}
