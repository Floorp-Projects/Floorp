# this is a dict of pool specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

config = {
    "taskcluster": {
        'upload_env': {
            'UPLOAD_PATH': '/builds/worker/artifacts',
        },
    },
}
