# This is a template config file for marionette production on Windows.
import os
import sys

config = {
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.ini",

    "virtualenv_python_dll": os.path.join(os.path.dirname(sys.executable), 'python27.dll'),
    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
        'virtualenv': [
            sys.executable,
            os.path.join(os.path.dirname(sys.executable), 'Lib', 'site-packages', 'virtualenv.py')
        ],
        'mozinstall': ['build/venv/scripts/python', 'build/venv/scripts/mozinstall-script.py'],
        'tooltool.py': [sys.executable, os.path.join(os.environ['MOZILLABUILD'], 'tooltool.py')],
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "proxxy": {},
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],
    "default_blob_upload_servers": [
         "https://blobupload.elasticbeanstalk.com",
    ],
    "blob_uploader_auth_file" : 'C:/builds/oauth.txt',
    "download_minidump_stackwalk": True,
    "download_symbols": "ondemand",
    "suite_definitions": {
        "gaiatest_desktop": {
            "options": [
                "--restart",
                "--timeout=%(timeout)s",
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--gecko-log=%(gecko_log)s",
                "--log-xunit=%(xml_output)s",
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
                "--testvars=%(testvars)s",
                "--profile=%(profile)s",
                "--symbols-path=%(symbols_path)s",
                "--log-xunit=%(xml_output)s",
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
