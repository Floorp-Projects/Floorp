import os

platform = "win64"

config = {
    "repack_id": os.environ.get("REPACK_ID"),

    'tooltool_url': 'https://tooltool.mozilla-releng.net/',
    'run_configure': False,

    'env': {
        'PATH': "%(abs_input_dir)s/upx/bin:%(PATH)s",
    }
}
