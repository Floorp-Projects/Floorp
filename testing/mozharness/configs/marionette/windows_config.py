# This is a template config file for marionette production on Windows.
import os
import sys

config = {
    # marionette options
    "test_type": "browser",
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.ini",

    "virtualenv_python_dll": 'c:/mozilla-build/python27/python27.dll',
    "virtualenv_path": 'venv',
    "exes": {
        'python': 'c:/mozilla-build/python27/python',
        'virtualenv': ['c:/mozilla-build/python27/python', 'c:/mozilla-build/buildbotve/virtualenv.py'],
        'hg': 'c:/mozilla-build/hg/hg',
        'mozinstall': ['%s/build/venv/scripts/python' % os.getcwd(),
                       '%s/build/venv/scripts/mozinstall-script.py' % os.getcwd()],
        'tooltool.py': [sys.executable, 'C:/mozilla-build/tooltool.py'],
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
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-marionette',
    ],
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : os.path.join(os.getcwd(), "oauth.txt"),
    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",
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
