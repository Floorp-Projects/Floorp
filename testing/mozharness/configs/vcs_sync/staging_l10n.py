from copy import deepcopy
import socket
hostname = socket.gethostname()

GECKO_BRANCHES = {
    'v2.0': 'mozilla-beta',
    'v2.1': 'mozilla-aurora',
    'v2.2': 'mozilla-central',
}

GECKO_CONFIG_TEMPLATE = {

    'mozilla-release': {
        'generate_git_notes': False, # we can change this when bug 1034725 is resolved
        'mapper': {
            'project': 'gitmo-gecko-l10n',
            'url': 'https://api-pub-build.allizom.org/mapper'
        },
        'locales_file_url': 'https://hg.mozilla.org/releases/mozilla-release/raw-file/default/b2g/locales/all-locales',
        'hg_url': 'https://hg.mozilla.org/releases/l10n/mozilla-release/%(locale)s',
        'targets': [{
            'target_dest': 'releases-l10n-%(locale)s-gecko/.git',
            'test_push': True,
            'vcs': 'git'
        }, {
            'target_dest': 'gitmo-gecko-l10n-%(locale)s',
        }],
        'tag_config': {
            'tag_regexes': [
                '^B2G_',
            ],
        },
    },

    'mozilla-beta': {
        'generate_git_notes': False, # we can change this when bug 1034725 is resolved
        'mapper': {
            'project': 'gitmo-gecko-l10n',
            'url': 'https://api-pub-build.allizom.org/mapper'
        },
        'locales_file_url': 'https://hg.mozilla.org/releases/mozilla-beta/raw-file/default/b2g/locales/all-locales',
        'hg_url': 'https://hg.mozilla.org/releases/l10n/mozilla-beta/%(locale)s',
        'targets': [{
            'target_dest': 'releases-l10n-%(locale)s-gecko/.git',
            'test_push': True,
            'vcs': 'git'
        }, {
            'target_dest': 'gitmo-gecko-l10n-%(locale)s',
        }],
        'tag_config': {
            'tag_regexes': [
                '^B2G_',
            ],
        },
    },

    'mozilla-aurora': {
        'generate_git_notes': False, # we can change this when bug 1034725 is resolved
        'mapper': {
            'project': 'gitmo-gecko-l10n',
            'url': 'https://api-pub-build.allizom.org/mapper'
        },
        'locales_file_url': 'https://hg.mozilla.org/releases/mozilla-aurora/raw-file/default/b2g/locales/all-locales',
        'hg_url': 'https://hg.mozilla.org/releases/l10n/mozilla-aurora/%(locale)s',
        'targets': [{
            'target_dest': 'releases-l10n-%(locale)s-gecko/.git',
            'test_push': True,
            'vcs': 'git'
        }, {
            'target_dest': 'gitmo-gecko-l10n-%(locale)s',
        }],
        'tag_config': {
            'tag_regexes': [
                '^B2G_',
            ],
        },
    },

    'mozilla-central': {
        'generate_git_notes': False, # we can change this when bug 1034725 is resolved
        'mapper': {
            'project': 'gitmo-gecko-l10n',
            'url': 'https://api-pub-build.allizom.org/mapper'
        },
        'locales_file_url': 'https://hg.mozilla.org/mozilla-central/raw-file/default/b2g/locales/all-locales',
        'hg_url': 'https://hg.mozilla.org/l10n-central/%(locale)s',
        'targets': [{
            'target_dest': 'releases-l10n-%(locale)s-gecko/.git',
            'test_push': True,
            'vcs': 'git'
        }, {
            'target_dest': 'gitmo-gecko-l10n-%(locale)s',
        }],
        'tag_config': {
            'tag_regexes': [
                '^B2G_',
            ],
        },
    },
}

# Build gecko_config
GECKO_CONFIG = {}
for version, branch in GECKO_BRANCHES.items():
    GECKO_CONFIG[branch] = deepcopy(GECKO_CONFIG_TEMPLATE[branch])
    GECKO_CONFIG[branch]['git_branch_name'] = version

config = {
    "log_name": "l10n",
    "log_max_rotate": 99,
    "job_name": "l10n",
    "env": {
        "PATH": "%(PATH)s:/usr/libexec/git-core",
    },
    "conversion_type": "b2g-l10n",
    "combined_mapfile": "l10n-mapfile",
    "l10n_config": {
        "gecko_config": GECKO_CONFIG,
        "gaia_config": {
            'v2_0': {
                'generate_git_notes': False, # we can change this when bug 1034725 is resolved
                'mapper': {
                    'project': 'gitmo-gaia-l10n',
                    'url': 'https://api-pub-build.allizom.org/mapper'
                },
                'locales_file_url': 'https://raw.github.com/mozilla-b2g/gaia/v2.0/locales/languages_all.json',
                'hg_url': 'https://hg.mozilla.org/releases/gaia-l10n/v2_0/%(locale)s',
                'git_branch_name': 'v2.0',
                'targets': [{
                    'target_dest': 'releases-l10n-%(locale)s-gaia/.git',
                    'test_push': True,
                    'vcs': 'git'
                }, {
                    'target_dest': 'gitmo-gaia-l10n-%(locale)s',
                }],
                'tag_config': {
                    'tag_regexes': [
                        '^B2G_',
                    ],
                },
            },
            'v1_4': {
                'generate_git_notes': False, # we can change this when bug 1034725 is resolved
                'mapper': {
                    'project': 'gitmo-gaia-l10n',
                    'url': 'https://api-pub-build.allizom.org/mapper'
                },
                'locales_file_url': 'https://raw.github.com/mozilla-b2g/gaia/v1.4/locales/languages_all.json',
                'hg_url': 'https://hg.mozilla.org/releases/gaia-l10n/v1_4/%(locale)s',
                'git_branch_name': 'v1.4',
                'targets': [{
                    'target_dest': 'releases-l10n-%(locale)s-gaia/.git',
                    'test_push': True,
                    'vcs': 'git'
                }, {
                    'target_dest': 'gitmo-gaia-l10n-%(locale)s',
                }],
                'tag_config': {
                    'tag_regexes': [
                        '^B2G_',
                    ],
                },
            },
            'v1_3': {
                'generate_git_notes': False, # we can change this when bug 1034725 is resolved
                'mapper': {
                    'project': 'gitmo-gaia-l10n',
                    'url': 'https://api-pub-build.allizom.org/mapper'
                },
                'locales_file_url': 'https://raw.github.com/mozilla-b2g/gaia/v1.3/locales/languages_dev.json',
                'hg_url': 'https://hg.mozilla.org/releases/gaia-l10n/v1_3/%(locale)s',
                'git_branch_name': 'v1.3',
                'targets': [{
                    'target_dest': 'releases-l10n-%(locale)s-gaia/.git',
                    'test_push': True,
                    'vcs': 'git'
                }, {
                    'target_dest': 'gitmo-gaia-l10n-%(locale)s',
                }],
                'tag_config': {
                    'tag_regexes': [
                        '^B2G_',
                    ],
                },
            },
            'v1_2': {
                'generate_git_notes': False, # we can change this when bug 1034725 is resolved
                'mapper': {
                    'project': 'gitmo-gaia-l10n',
                    'url': 'https://api-pub-build.allizom.org/mapper'
                },
                'locales_file_url': 'https://raw.github.com/mozilla-b2g/gaia/v1.2/locales/languages_all.json',
                'hg_url': 'https://hg.mozilla.org/releases/gaia-l10n/v1_2/%(locale)s',
                'git_branch_name': 'v1.2',
                'targets': [{
                    'target_dest': 'releases-l10n-%(locale)s-gaia/.git',
                    'test_push': True,
                    'vcs': 'git'
                }, {
                    'target_dest': 'gitmo-gaia-l10n-%(locale)s',
                }],
                'tag_config': {
                    'tag_regexes': [
                        '^B2G_',
                    ],
                },
            },
            'master': {
                'generate_git_notes': False, # we can change this when bug 1034725 is resolved
                'mapper': {
                    'project': 'gitmo-gaia-l10n',
                    'url': 'https://api-pub-build.allizom.org/mapper'
                },
                'locales_file_url': 'https://raw.github.com/mozilla-b2g/gaia/master/locales/languages_all.json',
                'hg_url': 'https://hg.mozilla.org/gaia-l10n/%(locale)s',
                'git_branch_name': 'master',
                'targets': [{
                    'target_dest': 'releases-l10n-%(locale)s-gaia/.git',
                    'test_push': True,
                    'vcs': 'git'
                }, {
                    'target_dest': 'gitmo-gaia-l10n-%(locale)s',
                }],
                'tag_config': {
                    'tag_regexes': [
                        '^B2G_',
                    ],
                },
            },
        },
    },

    "remote_targets": {
        "gitmo-gecko-l10n-%(locale)s": {
            "repo": 'git@github.com:petermoore/l10n-%(locale)s-gecko.git',
            "ssh_key": "~/.ssh/github_mozilla_rsa",
            "vcs": "git",
        },
        "gitmo-gaia-l10n-%(locale)s": {
            "repo": 'git@github.com:petermoore/l10n-%(locale)s-gaia.git',
            "ssh_key": "~/.ssh/github_mozilla_rsa",
            "vcs": "git",
        },
    },

    "virtualenv_modules": [
        "bottle==0.11.6",
        "dulwich==0.9.0",
        "ordereddict==1.1",
        "hg-git==0.4.0-moz2",
        "mapper==0.1",
        "mercurial==2.6.3",
        "mozfile==0.9",
        "mozinfo==0.5",
        "mozprocess==0.11",
        "requests==2.2.1",
    ],
    "find_links": [
        "http://pypi.pub.build.mozilla.org/pub",
    ],
    "pip_index": False,

    "default_notify_from": "developer-services@mozilla.org",
    "notify_config": [{
        "to": "pmoore@mozilla.com",
        "failure_only": False,
        "skip_empty_messages": True,
    }],

    # Disallow sharing, since we want pristine .hg and .git directories.
    "vcs_share_base": None,
    "hg_share_base": None,

    # any hg command line options
    "hg_options": (
        "--config",
        "web.cacerts=/etc/pki/tls/certs/ca-bundle.crt"
    ),

    "default_actions": [
        'list-repos',
        'create-virtualenv',
        'update-stage-mirror',
        'update-work-mirror',
        'publish-to-mapper',
        'push',
        'combine-mapfiles',
        'notify',
    ],
}
