# this is a dict of branch specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

# we should be able to port this in-tree and have the respective repos and
# revisions handle what goes on in here. Tracking: bug 978510

# example config and explanation of how it works:
# config = {
#     # if a branch matches a key below, override items in self.config with
#     # items in the key's value.
#     # this override can be done for every platform or at a platform level
#     '<branch-name>': {
#         # global config items (applies to all platforms and build types)
#         'repo_path': "projects/<branch-name>",
#
#         # platform config items (applies to specific platforms)
#         'platform_overrides': {
#             # if a platform matches a key below, override items in
#             # self.config with items in the key's value
#             'linux64-debug': {
#                 'upload_symbols': False,
#             },
#             'win64': {
#                 'enable_checktests': False,
#             },
#         }
#     },
# }

config = {
    ### release branches
    "mozilla-central": {
        "repo_path": 'mozilla-central',
        "update_channel": "nightly",
    },
    'mozilla-release': {
        'enable_release_promotion': True,
        'repo_path': 'releases/mozilla-release',
        'update_channel': 'release',
        'branch_uses_per_checkin_strategy': True,
        'platform_overrides': {
            'linux': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'linux64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'macosx64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'win32': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'win64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'linux-debug': {
                'update_channel': 'default',
            },
            'linux64-debug': {
                'update_channel': 'default',
            },
            'linux64-asan-debug': {
                'update_channel': 'default',
            },
            'linux64-asan': {
                'update_channel': 'default',
            },
            'linux64-st-an-debug': {
                'update_channel': 'default',
            },
            'linux64-st-an': {
                'update_channel': 'default',
            },
            'linux64-add-on-devel': {
                'update_channel': 'default',
            },
            'macosx64-debug': {
                'update_channel': 'default',
            },
            'macosx64-st-an': {
                'update_channel': 'default',
            },
            'macosx64-st-an-debug': {
                'update_channel': 'default',
            },
            'macosx64-add-on-devel': {
                'update_channel': 'default',
            },
            'win32-debug': {
                'update_channel': 'default',
            },
            'win32-add-on-devel': {
                'update_channel': 'default',
            },
            'win64-debug': {
                'update_channel': 'default',
            },
            'win64-add-on-devel': {
                'update_channel': 'default',
            },
        },
    },
    'mozilla-esr60': {
        'enable_release_promotion': True,
        'repo_path': 'releases/mozilla-esr60',
        'update_channel': 'esr',
        'branch_uses_per_checkin_strategy': True,
        'platform_overrides': {
            'linux': {
                # We keep using the release configs as the beta and release configs are
                # identical except for
                # https://searchfox.org/mozilla-central/rev/ce9ff94ffed34dc17ec0bfa406156d489eaa8ee1/browser/config/mozconfigs/linux32/release#1    # noqa
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'linux64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'macosx64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'win32': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'win64': {
                'mozconfig_variant': 'release',
                'force_clobber': True,
            },
            'linux-debug': {
                'update_channel': 'default',
            },
            'linux64-debug': {
                'update_channel': 'default',
            },
            'linux64-asan-debug': {
                'update_channel': 'default',
            },
            'linux64-asan': {
                'update_channel': 'default',
            },
            'linux64-st-an-debug': {
                'update_channel': 'default',
            },
            'linux64-st-an': {
                'update_channel': 'default',
            },
            'linux64-add-on-devel': {
                'update_channel': 'default',
            },
            'macosx64-debug': {
                'update_channel': 'default',
            },
            'macosx64-st-an': {
                'update_channel': 'default',
            },
            'macosx64-st-an-debug': {
                'update_channel': 'default',
            },
            'macosx64-add-on-devel': {
                'update_channel': 'default',
            },
            'win32-debug': {
                'update_channel': 'default',
            },
            'win32-add-on-devel': {
                'update_channel': 'default',
            },
            'win64-debug': {
                'update_channel': 'default',
            },
            'win64-add-on-devel': {
                'update_channel': 'default',
            },
        },
    },
    'mozilla-beta': {
        'enable_release_promotion': 1,
        'repo_path': 'releases/mozilla-beta',
        'update_channel': 'beta',
        'branch_uses_per_checkin_strategy': True,
        'platform_overrides': {
            'linux': {
                'mozconfig_variant': 'beta',
                'force_clobber': True,
            },
            'linux64': {
                'mozconfig_variant': 'beta',
                'force_clobber': True,
            },
            'macosx64': {
                'mozconfig_variant': 'beta',
                'force_clobber': True,
            },
            'win32': {
                'mozconfig_variant': 'beta',
                'force_clobber': True,
            },
            'win64': {
                'mozconfig_variant': 'beta',
                'force_clobber': True,
            },
            'linux-devedition': {
                "update_channel": "aurora",
            },
            'linux64-devedition': {
                "update_channel": "aurora",
            },
            'macosx64-devedition': {
                "update_channel": "aurora",
            },
            'win32-devedition': {
                "update_channel": "aurora",
            },
            'win64-devedition': {
                "update_channel": "aurora",
            },
            'linux-debug': {
                'update_channel': 'default',
            },
            'linux64-debug': {
                'update_channel': 'default',
            },
            'linux64-asan-debug': {
                'update_channel': 'default',
            },
            'linux64-asan': {
                'update_channel': 'default',
            },
            'linux64-st-an-debug': {
                'update_channel': 'default',
            },
            'linux64-st-an': {
                'update_channel': 'default',
            },
            'linux64-add-on-devel': {
                'update_channel': 'default',
            },
            'macosx64-debug': {
                'update_channel': 'default',
            },
            'macosx64-st-an': {
                'update_channel': 'default',
            },
            'macosx64-st-an-debug': {
                'update_channel': 'default',
            },
            'macosx64-add-on-devel': {
                'update_channel': 'default',
            },
            'win32-debug': {
                'update_channel': 'default',
            },
            'win32-add-on-devel': {
                'update_channel': 'default',
            },
            'win64-debug': {
                'update_channel': 'default',
            },
            'win64-add-on-devel': {
                'update_channel': 'default',
            },
        },
    },
    'mozilla-aurora': {
        'repo_path': 'releases/mozilla-aurora',
        'update_channel': 'aurora',
        'branch_uses_per_checkin_strategy': True,
    },
    'try': {
        'repo_path': 'try',
        'branch_supports_uploadsymbols': False,
        'use_clobberer': False,
    },

    ### project branches
    #'fx-team': {},   #Bug 1296396
    'gum': {
        'branch_uses_per_checkin_strategy': True,
    },
    'mozilla-inbound': {
        'repo_path': 'integration/mozilla-inbound',
    },
    'autoland': {
        'repo_path': 'integration/autoland',
    },
    'ux': {},
    'cypress': {
        # bug 1164935
        'branch_uses_per_checkin_strategy': True,
    },

    ### other branches that do not require anything special:
    'alder': {},
    'ash': {},
    'birch': {},
    # 'build-system': {}
    'cedar': {},
    'elm': {},
    'fig': {},
    'graphics': {},
    # 'holly': {},
    'jamun': {
        'update_channel': 'beta',
        'enable_release_promotion': 1,
        'platform_overrides': {
            'linux': {
                'mozconfig_variant': 'release',
            },
            'linux-debug': {
                'update_channel': 'default',
            },
            'linux64': {
                'mozconfig_variant': 'release',
            },
            'linux64-debug': {
                'update_channel': 'default',
            },
            'linux64-asan-debug': {
                'update_channel': 'default',
            },
            'linux64-asan': {
                'update_channel': 'default',
            },
            'linux64-st-an-debug': {
                'update_channel': 'default',
            },
            'linux64-st-an': {
                'update_channel': 'default',
            },
            'macosx64': {
                'mozconfig_variant': 'release',
            },
            'macosx64-debug': {
                'update_channel': 'default',
            },
            'macosx64-st-an': {
                'update_channel': 'default',
            },
            'macosx64-st-an-debug': {
                'update_channel': 'default',
            },
            'win32': {
                'mozconfig_variant': 'release',
            },
            'win32-debug': {
                'update_channel': 'default',
            },
            'win64': {
                'mozconfig_variant': 'release',
            },
            'win64-debug': {
                'update_channel': 'default',
            },
            'linux-devedition': {
                "update_channel": "aurora",
            },
            'linux64-devedition': {
                "update_channel": "aurora",
            },
            'macosx64-devedition': {
                "update_channel": "aurora",
            },
            'win32-devedition': {
                "update_channel": "aurora",
            },
            'win64-devedition': {
                "update_channel": "aurora",
            },
        },
    },
    'larch': {},
    # 'maple': {},
    'oak': {},
    'pine': {},
}
