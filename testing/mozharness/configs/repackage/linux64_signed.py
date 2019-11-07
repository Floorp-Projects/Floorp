import os

platform = "linux64"

tooltool_url = 'http://taskcluster/tooltool.mozilla-releng.net/'
if os.environ.get('TASKCLUSTER_ROOT_URL', 'https://taskcluster.net') != 'https://taskcluster.net':
    # Pre-point tooltool at staging cluster so we can run in parallel with legacy cluster
    tooltool_url = 'http://taskcluster/stage.tooltool.mozilla-releng.net/'

config = {
    "locale": os.environ.get("LOCALE"),

    # ToolTool
    "tooltool_url": tooltool_url,
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
