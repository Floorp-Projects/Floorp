import os
import platform
import sys

# OS Specifics
ABS_WORK_DIR = os.path.join(os.getcwd(), "build")
BINARY_PATH = os.path.join(ABS_WORK_DIR, "firefox", "firefox.exe")
INSTALLER_PATH = os.path.join(ABS_WORK_DIR, "installer.zip")
XPCSHELL_NAME = 'xpcshell.exe'
EXE_SUFFIX = '.exe'
DISABLE_SCREEN_SAVER = False
ADJUST_MOUSE_AND_SCREEN = True
DESKTOP_VISUALFX_THEME = {
    'Let Windows choose': 0,
    'Best appearance': 1,
    'Best performance': 2,
    'Custom': 3
}.get('Best appearance')
TASKBAR_AUTOHIDE_REG_PATH = {
    'Windows 7': 'HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StuckRects2',
    'Windows 10': 'HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\StuckRects3'
}.get('{} {}'.format(platform.system(), platform.release()))
#####
config = {
    "exes": {
        'python': sys.executable,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },
    ###
    "installer_path": INSTALLER_PATH,
    "binary_path": BINARY_PATH,
    "xpcshell_name": XPCSHELL_NAME,
    "virtualenv_modules": ['pypiwin32'],
    "virtualenv_path": 'venv',

    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,
    "exe_suffix": EXE_SUFFIX,
    "run_file_names": {
        "mochitest": "runtests.py",
        "reftest": "runreftest.py",
        "xpcshell": "runxpcshelltests.py",
        "cppunittest": "runcppunittests.py",
        "gtest": "rungtests.py",
        "jittest": "jit_test.py",
        "mozbase": "test.py",
        "mozmill": "runtestlist.py",
    },
    "minimum_tests_zip_dirs": [
        "bin/*",
        "certs/*",
        "config/*",
        "mach",
        "marionette/*",
        "modules/*",
        "mozbase/*",
        "tools/*",
        "mozpack/*",
        "mozbuild/*",
    ],
    "suite_definitions": {
        "cppunittest": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--xre-path=%(abs_app_dir)s"
            ],
            "run_filename": "runcppunittests.py",
            "testsdir": "cppunittest"
        },
        "jittest": {
            "options": [
                "tests/bin/js",
                "--no-slow",
                "--no-progress",
                "--format=automation",
                "--jitflags=all",
                "--timeout=970" # Keep in sync with run_timeout below.
            ],
            "run_filename": "jit_test.py",
            "testsdir": "jit-test/jit-test",
            "run_timeout": 1000 # Keep in sync with --timeout above.
        },
        "mochitest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--certificate-path=tests/certs",
                "--quiet",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--screenshot-on-fail",
                "--cleanup-crashes",
                "--marionette-startup-timeout=180",
            ],
            "run_filename": "runtests.py",
            "testsdir": "mochitest"
        },
        "mozbase": {
            "options": [
                "-b",
                "%(binary_path)s"
            ],
            "run_filename": "test.py",
            "testsdir": "mozbase"
        },
        "mozmill": {
            "options": [
                "--binary=%(binary_path)s",
                "--testing-modules-dir=test/modules",
                "--plugins-path=%(test_plugin_path)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "runtestlist.py",
            "testsdir": "mozmill"
        },
        "reftest": {
            "options": [
                "--appname=%(binary_path)s",
                "--utility-path=tests/bin",
                "--extra-profile-file=tests/bin/plugins",
                "--symbols-path=%(symbols_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--cleanup-crashes",
                "--marionette-startup-timeout=180",
                "--sandbox-read-whitelist=%(abs_work_dir)s",
            ],
            "run_filename": "runreftest.py",
            "testsdir": "reftest"
        },
        "xpcshell": {
            "options": [
                "--symbols-path=%(symbols_path)s",
                "--test-plugin-path=%(test_plugin_path)s",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--utility-path=tests/bin",
            ],
            "run_filename": "runxpcshelltests.py",
            "testsdir": "xpcshell"
        },
        "gtest": {
            "options": [
                "--xre-path=%(abs_res_dir)s",
                "--cwd=%(gtest_dir)s",
                "--symbols-path=%(symbols_path)s",
                "--utility-path=tests/bin",
                "%(binary_path)s",
            ],
            "run_filename": "rungtests.py",
        },
    },
    # local mochi suites
    "all_mochitest_suites":
    {
        "plain": [],
        "plain-gpu": ["--subsuite=gpu"],
        "plain-clipboard": ["--subsuite=clipboard"],
        "plain-chunked": ["--chunk-by-dir=4"],
        "mochitest-media": ["--subsuite=media"],
        "chrome": ["--flavor=chrome"],
        "chrome-gpu": ["--flavor=chrome", "--subsuite=gpu"],
        "chrome-clipboard": ["--flavor=chrome", "--subsuite=clipboard"],
        "chrome-chunked": ["--flavor=chrome", "--chunk-by-dir=4"],
        "browser-chrome": ["--flavor=browser"],
        "browser-chrome-gpu": ["--flavor=browser", "--subsuite=gpu"],
        "browser-chrome-clipboard": ["--flavor=browser", "--subsuite=clipboard"],
        "browser-chrome-chunked": ["--flavor=browser", "--chunk-by-runtime"],
        "browser-chrome-addons": ["--flavor=browser", "--chunk-by-runtime", "--tag=addons"],
        "browser-chrome-screenshots": ["--flavor=browser", "--subsuite=screenshots"],
        "browser-chrome-instrumentation": ["--flavor=browser"],
        "mochitest-gl": ["--subsuite=webgl"],
        "mochitest-devtools-chrome": ["--flavor=browser", "--subsuite=devtools"],
        "mochitest-devtools-chrome-chunked": ["--flavor=browser", "--subsuite=devtools", "--chunk-by-runtime"],
        "mochitest-metro-chrome": ["--flavor=browser", "--metro-immersive"],
        "a11y": ["--flavor=a11y"],
    },
    # local reftest suites
    "all_reftest_suites": {
        "crashtest": {
            'options': ["--suite=crashtest"],
            'tests': ["tests/reftest/tests/testing/crashtest/crashtests.list"]
        },
        "jsreftest": {
            'options':["--extra-profile-file=tests/jsreftest/tests/user.js",
                       "--suite=jstestbrowser"],
            'tests': ["tests/jsreftest/tests/jstests.list"]
        },
        "reftest": {
            'options': ["--suite=reftest"],
            'tests': ["tests/reftest/tests/layout/reftests/reftest.list"]
        },
        "reftest-gpu": {
            'options': ["--suite=reftest",
                        "--setpref=layers.gpu-process.force-enabled=true"],
            'tests': ["tests/reftest/tests/layout/reftests/reftest.list"]
        },
        "reftest-no-accel": {
            "options": ["--suite=reftest",
                        "--setpref=layers.acceleration.disabled=true"],
            "tests": ["tests/reftest/tests/layout/reftests/reftest.list"]
        },
    },
    "all_xpcshell_suites": {
        "xpcshell": {
            'options': ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--manifest=tests/xpcshell/tests/xpcshell.ini"],
            'tests': []
        },
        "xpcshell-addons": {
            'options': ["--xpcshell=%(abs_app_dir)s/" + XPCSHELL_NAME,
                        "--tag=addons",
                        "--manifest=tests/xpcshell/tests/xpcshell.ini"],
            'tests': []
        },
    },
    "all_cppunittest_suites": {
        "cppunittest": ['tests/cppunittest']
    },
    "all_gtest_suites": {
        "gtest": []
    },
    "all_jittest_suites": {
        "jittest": [],
        "jittest-chunked": [],
    },
    "all_mozbase_suites": {
        "mozbase": []
    },
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        {
            'name': 'disable_screen_saver',
            'cmd': ['xset', 's', 'off', 's', 'reset'],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': False,
            'enabled': DISABLE_SCREEN_SAVER
        },
        {
            'name': 'run mouse & screen adjustment script',
            'cmd': [
                sys.executable,
                os.path.join(os.getcwd(),
                    'mozharness', 'external_tools', 'mouse_and_screen_resolution.py'),
                '--configuration-file',
                os.path.join(os.getcwd(),
                    'mozharness', 'external_tools', 'machine-configuration.json')
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': ADJUST_MOUSE_AND_SCREEN
        },
        {
            'name': 'disable windows security and maintenance notifications',
            'cmd': [
                'powershell', '-command',
                '"&{$p=\'HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Notifications\Settings\Windows.SystemToast.SecurityAndMaintenance\';if(!(Test-Path -Path $p)){&New-Item -Path $p -Force}&Set-ItemProperty -Path $p -Name Enabled -Value 0}"'
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': (platform.release() == 10)
        },
        {
            'name': 'set windows VisualFX',
            'cmd': [
                'powershell', '-command',
                '"&{{&Set-ItemProperty -Path \'HKCU:Software\Microsoft\Windows\CurrentVersion\Explorer\VisualEffects\' -Name VisualFXSetting -Value {}}}"'.format(DESKTOP_VISUALFX_THEME)
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': True
        },
        {
            'name': 'hide windows taskbar',
            'cmd': [
                'powershell', '-command',
                '"&{{$p=\'{}\';$v=(Get-ItemProperty -Path $p).Settings;$v[8]=3;&Set-ItemProperty -Path $p -Name Settings -Value $v}}"'.format(TASKBAR_AUTOHIDE_REG_PATH)
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': True
        },
        {
            'name': 'restart windows explorer',
            'cmd': [
                'powershell', '-command',
                '"&{&Stop-Process -ProcessName explorer}"'
            ],
            'architectures': ['32bit', '64bit'],
            'halt_on_failure': True,
            'enabled': True
        },
    ],
    "vcs_output_timeout": 1000,
    "minidump_save_path": "%(abs_work_dir)s/../minidumps",
    "buildbot_max_log_size": 52428800,
    "default_blob_upload_servers": [
        "https://blobupload.elasticbeanstalk.com",
    ],
    "structured_suites": ["reftest"],
    'blob_uploader_auth_file': 'C:/builds/oauth.txt',
    "download_minidump_stackwalk": True,
    "minidump_stackwalk_path": "win32-minidump_stackwalk.exe",
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/win32/releng.manifest",
    "download_nodejs": True,
    "nodejs_path": "node-win32.exe",
    "nodejs_tooltool_manifest_path": "config/tooltool-manifests/win32/nodejs.manifest",
}
