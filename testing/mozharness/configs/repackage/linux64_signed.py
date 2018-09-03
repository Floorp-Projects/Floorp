import os

platform = "linux64"

config = {
    "locale": os.environ.get("LOCALE"),

    # ToolTool
    "tooltool_url": 'http://relengapi/tooltool/',
    'tooltool_cache': os.environ.get('TOOLTOOL_CACHE'),

    'run_configure': False,
}
