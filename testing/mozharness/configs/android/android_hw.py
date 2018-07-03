# mozharness configuration for Android hardware unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "robocop_package_name": "org.mozilla.roboexample.test",
    "marionette_address": "%(device_ip)s:2828",
    "exes": {},
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s",
        "MINIDUMP_SAVEPATH": "%(abs_work_dir)s/../minidumps"
    },
    "default_actions": [
        'clobber',
        'download-and-extract',
        'create-virtualenv',
        'verify-device',
        'install',
        'run-tests',
    ],
    # from android_common.py
    "download_tooltool": True,
    "download_minidump_stackwalk": True,
    "tooltool_servers": ['https://api.pub.build.mozilla.org/tooltool/'],
    # minidump_tooltool_manifest_path is relative to workspace/build/tests/
    "minidump_tooltool_manifest_path": "config/tooltool-manifests/linux64/releng.manifest",
    "xpcshell_extra": "--remoteTestRoot=/data/local/tests",
}
