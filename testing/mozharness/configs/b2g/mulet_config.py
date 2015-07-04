# Call --cfg b2g/generic_config.py before this config
config = {
    "default_actions": [
        'clobber',
        'read-buildbot-config',
        'pull',
        'download-and-extract',
        'create-virtualenv',
        'install',
        'run-tests',
    ],
    "in_tree_config": "config/mozharness/linux_mulet_config.py",
}
