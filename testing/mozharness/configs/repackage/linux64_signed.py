import os

platform = "linux64"

config = {
    "locale": os.environ.get("LOCALE"),

    # ToolTool
    "tooltool_url": 'http://taskcluster/tooltool.mozilla-releng.net/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
