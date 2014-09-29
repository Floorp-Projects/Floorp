# This is used by mozharness' mulet_unittest.py
config = {
    # testsuite options
    "reftest_options": [
        "--desktop", "--profile=%(gaia_profile)s",
        "--appname=%(application)s", "%(test_manifest)s"
    ],
    "run_file_names": {
        "reftest": "runreftestb2g.py",
    },
}
