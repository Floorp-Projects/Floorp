# This is a template config file for marionette production.
import os
import platform


HG_SHARE_BASE_DIR = "/builds/hg-shared"

if platform.system().lower() == 'darwin':
    xre_url = "https://api.pub.build.mozilla.org/tooltool/sha512/4d8d7a37d90c34a2a2fda3066a8fe85c189b183d05389cb957fc6fed31f10a6924e50c1b84488ff61c015293803f58a3aed5d4819374d04c8e0ee2b9e3997278"
else:
    xre_url = "https://api.pub.build.mozilla.org/tooltool/sha512/dc9503b21c87b5a469118746f99e4f41d73888972ce735fa10a80f6d218086da0e3da525d9a4cd8e4ea497ec199fef720e4a525873d77a1af304ac505e076462"

config = {
    # marionette options
    "test_type": "b2g",
    "marionette_address": "localhost:2828",
    "gaiatest": True,
    "xre_url": xre_url,
    "application": "b2g",

    "vcs_share_base": HG_SHARE_BASE_DIR,
    "exes": {
        'python': '/tools/buildbot/bin/python',
        'virtualenv': ['/tools/buildbot/bin/python', '/tools/misc-python/virtualenv.py'],
        'tooltool.py': "/tools/tooltool.py",
    },

    "find_links": [
        "http://pypi.pvt.build.mozilla.org/pub",
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "buildbot_json_path": "buildprops.json",

    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'pull',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-marionette',
    ],
    "download_symbols": "ondemand",
    "download_minidump_stackwalk": True,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file": os.path.join(os.getcwd(), "oauth.txt"),
    "vcs_output_timeout": 1760,
    "tooltool_cache": "/builds/tooltool_cache",
    "suite_definitions": {
        "gaiatest_desktop": {
            "options": [
                "--restart",
                "--timeout=%(timeout)s",
                "--type=%(type)s",
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--gecko-log=%(gecko_log)s",
                "--xml-output=%(xml_output)s",
                "--html-output=%(html_output)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--total-chunks=%(total_chunks)s",
                "--this-chunk=%(this_chunk)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "gaiatest_emulator": {
            "options": [
                "--restart",
                "--timeout=%(timeout)s",
                "--type=%(type)s",
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--xml-output=%(xml_output)s",
                "--html-output=%(html_output)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "marionette_desktop": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "marionette_emulator": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "",
            "testsdir": ""
        },
        "webapi_desktop": {
            "options": [],
            "run_filename": "",
            "testsdir": ""
        },
        "webapi_emulator": {
            "options": [
                "--type=%(type)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--symbols-path=%(symbols_path)s",
                "--logcat-dir=%(logcat_dir)s",
                "--emulator=%(emulator)s",
                "--homedir=%(homedir)s"
            ],
            "run_filename": "",
            "testsdir": ""
        }
    },
}
