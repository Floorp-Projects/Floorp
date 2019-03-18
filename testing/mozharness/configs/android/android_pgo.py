# Mozharness configuration for Android PGO.
#
# This configuration should be combined with platform-specific mozharness
# configuration such as androidarm_4_3.py, or similar.

config = {
    "default_actions": [
        'setup-avds',
        'start-emulator',
        'download',
        'create-virtualenv',
        'verify-device',
        'install',
        'run-tests',
    ],
    "output_directory": "/sdcard",
}
