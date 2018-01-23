# this is a dict of pool specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

config = {
    "staging": {
        # if not clobberer_url, only clobber 'abs_work_dir'
        # if true: possibly clobber, clobberer
        # see PurgeMixin for clobber() conditions
        'clobberer_url': 'https://api-pub-build.allizom.org/clobberer/lastclobber',
        # staging we should use MozillaTest
        # but in production we let the self.branch decide via
        # self._query_graph_server_branch_name()
        "graph_server_branch_name": "MozillaTest",
        'graph_server': 'graphs.allizom.org',
        'stage_server': 'upload.ffxbld.productdelivery.stage.mozaws.net',
        'taskcluster_index': 'index.garbage.staging',
        'post_upload_extra': ['--bucket-prefix', 'net-mozaws-stage-delivery',
                              '--url-prefix', 'http://ftp.stage.mozaws.net/',
                              ],
    },
    "production": {
        # if not clobberer_url, only clobber 'abs_work_dir'
        # if true: possibly clobber, clobberer
        # see PurgeMixin for clobber() conditions
        'clobberer_url': 'https://api.pub.build.mozilla.org/clobberer/lastclobber',
        'graph_server': 'graphs.mozilla.org',
        # bug 1216907, set this at branch level
        # 'stage_server': 'upload.ffxbld.productdelivery.prod.mozaws.net',
        'taskcluster_index': 'index',
    },
    "taskcluster": {
        'graph_server': 'graphs.mozilla.org',
        'stage_server': 'ignored',
        # use the relengapi proxy to talk to tooltool
        "tooltool_servers": ['http://relengapi/tooltool/'],
        "tooltool_url": 'http://relengapi/tooltool/',
        'upload_env': {
            'UPLOAD_HOST': 'localhost',
            'UPLOAD_PATH': '/builds/worker/artifacts',
        },
    },
}
