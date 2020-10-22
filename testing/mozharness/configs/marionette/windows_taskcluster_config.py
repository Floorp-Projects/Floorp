# This is a template config file for marionette production on Windows.
import os
import platform
import sys

# OS Specifics
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
    # marionette options
    "marionette_address": "localhost:2828",
    "test_manifest": "unit-tests.ini",

    "virtualenv_path": 'venv',
    "exes": {
        'python': sys.executable,
        'hg': os.path.join(os.environ['PROGRAMFILES'], 'Mercurial', 'hg')
    },

    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],
    "download_symbols": "ondemand",
    "suite_definitions": {
        "marionette_desktop": {
            "options": [
                "-vv",
                "--log-raw=%(raw_log_file)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--log-html=%(html_report_file)s",
                "--binary=%(binary)s",
                "--address=%(address)s",
                "--symbols-path=%(symbols_path)s"
            ],
            "run_filename": "",
            "testsdir": "marionette"
        },
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
                '"&{$p=\'HKCU:SOFTWARE\Microsoft\Windows\CurrentVersion\Notifications\Settings\Windows.SystemToast.SecurityAndMaintenance\';if(!(Test-Path -Path $p)){&New-Item -Path $p -Force}&Set-ItemProperty -Path $p -Name Enabled -Value 0}"'  # noqa
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
}
