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
    },
    'mozilla-release': {
        'enable_release_promotion': True,
        'branch_uses_per_checkin_strategy': True,
    },
    'mozilla-esr60': {
        'enable_release_promotion': True,
        'branch_uses_per_checkin_strategy': True,
    },
    'mozilla-beta': {
        'enable_release_promotion': 1,
        'branch_uses_per_checkin_strategy': True,
    },
    'try': {
        'branch_supports_uploadsymbols': False,
    },

    ### project branches
    #'fx-team': {},   #Bug 1296396
    'gum': {
        'branch_uses_per_checkin_strategy': True,
    },
    'mozilla-inbound': {
    },
    'autoland': {
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
        'enable_release_promotion': 1,
    },
    'larch': {},
    # 'maple': {},
    'oak': {},
    'pine': {},
}
