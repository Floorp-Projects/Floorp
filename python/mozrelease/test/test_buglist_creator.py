import unittest
from pkg_resources import parse_version
from kickoff.buglist_creator import is_excluded_change, create_bugs_url, is_backout_bug, get_previous_tag_version, \
    get_bugs_in_changeset, dot_version_to_tag_version, tag_version_to_dot_version_parse


class TestBuglistCreator(unittest.TestCase):

    def test_beta_1_release(self):
        release_object_54_0b1 = {
            'branch': 'releases/mozilla-beta',
            'product': 'firefox',
            'version': '54.0b1',
            'mozillaRevision': 'cf76e00dcd6f',
        }
        buglist_str_54_0b1 = create_bugs_url(release_object_54_0b1)
        assert buglist_str_54_0b1 == '', 'There should be no bugs to compare for beta 1.'

    def test_is_excluded_change(self):
        excluded_changesets = [
            {'desc': 'something something something a=test-only something something something'},
            {'desc': 'this is a a=release change!'},
        ]
        assert all(is_excluded_change(excluded) for excluded in excluded_changesets), 'is_excluded_change failed to exclude a changeset.'

    def test_is_backout_bug(self):
        backout_bugs_descs = [
            'I backed out this bug because',
            'Backing out this bug due to',
            'Backout bug xyz',
            'Back out bug xyz',
        ]

        not_backout_bugs = [
            'this is a regular bug description',
        ]

        assert all(is_backout_bug(backout_desc.lower()) for backout_desc in backout_bugs_descs)
        assert all(not is_backout_bug(regular_desc.lower()) for regular_desc in not_backout_bugs)

    def test_dot_version_to_tag_version(self):
        test_tuples = [
            (['firefox', '53.0b10'], 'FIREFOX_53_0b10_RELEASE'),
            (['firefox', '52.0'], 'FIREFOX_52_0_RELEASE'),
            (['fennec', '52.0.2'], 'FENNEC_52_0_2_RELEASE'),
        ]

        assert all(dot_version_to_tag_version(*args) == results for args, results in test_tuples)

    def test_tag_version_to_dot_version_parse(self):
        test_tuples = [
            ('FIREFOX_53_0b10_RELEASE', parse_version('53.0b10')),
            ('FIREFOX_52_0_RELEASE', parse_version('52.0')),
            ('FENNEC_52_0_2_RELEASE', parse_version('52.0.2')),
        ]

        assert all(tag_version_to_dot_version_parse(tag) == expected for tag, expected in test_tuples)

    def test_get_previous_tag_version(self):
        product = 'firefox'
        ff_48_tags = [
            u'FIREFOX_BETA_48_END',
            u'FIREFOX_RELEASE_48_END',
            u'FIREFOX_48_0_2_RELEASE',
            u'FIREFOX_48_0_2_BUILD1',
            u'FIREFOX_48_0_1_RELEASE',
            u'FIREFOX_48_0_1_BUILD3',
            u'FIREFOX_48_0_RELEASE',
            u'FIREFOX_48_0_BUILD2',
            u'FIREFOX_RELEASE_48_BASE',
            u'FIREFOX_48_0b10_RELEASE',
            u'FIREFOX_48_0b10_BUILD1',
            u'FIREFOX_48_0b9_RELEASE',
            u'FIREFOX_48_0b9_BUILD1',
            u'FIREFOX_48_0b7_RELEASE',
            u'FIREFOX_48_0b7_BUILD1',
            u'FIREFOX_48_0b6_RELEASE',
            u'FIREFOX_48_0b6_BUILD1',
            u'FIREFOX_48_0b5_RELEASE',
            u'FIREFOX_48_0b5_BUILD1',
            u'FIREFOX_48_0b4_RELEASE',
            u'FIREFOX_48_0b4_BUILD1',
            u'FIREFOX_48_0b3_RELEASE',
            u'FIREFOX_48_0b3_BUILD1',
            u'FIREFOX_48_0b2_RELEASE',
            u'FIREFOX_48_0b2_BUILD2',
            u'FIREFOX_48_0b1_RELEASE',
            u'FIREFOX_48_0b1_BUILD2',
            u'FIREFOX_AURORA_48_END',
            u'FIREFOX_BETA_48_BASE',
            u'FIREFOX_AURORA_48_BASE',
        ]

        mock_hg_json = {
            'tags': [
                {'tag': tag} for tag in ff_48_tags
            ]
        }

        test_tuples = [
            ('48.0b4', 'FIREFOX_48_0b4_RELEASE', 'FIREFOX_48_0b3_RELEASE'),
            ('48.0b9', 'FIREFOX_48_0b9_RELEASE', 'FIREFOX_48_0b7_RELEASE'),
            ('48.0.2', 'FIREFOX_48_0_2_RELEASE', 'FIREFOX_48_0_1_RELEASE'),
            ('48.0.1', 'FIREFOX_48_0_1_RELEASE', 'FIREFOX_48_0_RELEASE'),
        ]

        assert all(get_previous_tag_version(product, dot_version, tag_version, mock_hg_json) == expected
                   for dot_version, tag_version, expected in test_tuples)

    def test_get_bugs_in_changeset(self):
        changeset_data = {
            0: {'changesets': [{'desc': u'Bug 1354038 - [push-apk] taskgraph: Use rollout and deactivate dry-run on release p=jlorenzo r=aki a=release DONTBUILD'}]},
            1: {'changesets': [{'desc': u'Bug 1356563 - Only set global ready state on native widget loading; r=snorp a=sylvestre\n\nOur "chrome-document-loaded" observer may detect several different types\nof widgets that can exist in the parent process, including the Android\nnsWindow, PuppetWidget, etc. We should only set the global state to\nready when the first top-level nsWindow has loaded, and not just any\nwindow.'}]},
            2: {'changesets': [{'desc': u'No bug, Automated blocklist update from host bld-linux64-spot-305 - a=blocklist-update'}]},
            3: {'changesets': [{'desc': u'Automatic version bump. CLOSED TREE NO BUG a=release'},
                               {'desc': u'No bug - Tagging d345b657d381ade5195f1521313ac651618f54a2 with FIREFOX_53_0_BUILD6, FIREFOX_53_0_RELEASE a=release CLOSED TREE'}]},
            4: {'changesets': [{'desc': u'No bug, Automated blocklist update from host bld-linux64-spot-305 - a=blocklist-update'}]},
            5: {'changesets': [{'desc': u'Bug 1344529 - Remove unused variable in widget/gtk/gtk2drawing.c. r=frg a=release DONOTBUILD in a CLOSED TREE'},
                               {'desc': u'Bug 1306543 - Avoid using g_unicode_script_from_iso15924 directly. r=jfkthame a=release in a CLOSED TREE DONTBUILD'}]},
            6: {'changesets': [{'desc': u'Bug 1320072 - Backout intent change - broke partner Google test. r=snorp, a=lizzard'}]},
            7: {'changesets': [{'desc': u'Bug 1328762 - Cherry-pick ANGLE a4aaa2de57dc51243da35ea147d289a21a9f0c49. a=lizzard\n\nMozReview-Commit-ID: WVK0smAfAW'},
                               {'desc': u'Bug 1341190 - Remove .popup-anchor visibility rule. r=mconley, a=lizzard\n\nMozReview-Commit-ID: DFMIKMMnLx5'},
                               {'desc': u'Bug 1348409 - Stop supporting the showDialog argument for window.find. r=mrbkap, a=lizzard\n\nThe dialog functionality of the non-standard window.find API has been broken\nwith e10s since it shipped, and bug 1182569 or bug 1232432 (or both) have\nbroken it for non-e10s.\n\nThis patch remove showDialog support entirely, for both e10s and non-e10s,\nin a more deliberate way. We now ignore the argument.\n\nMozReview-Commit-ID: 1CTzgEkDhHW'},
                               {'desc': u'Bug 1358089 - [RTL] Separate xml drawable into v17 folder. r=ahunt, a=lizzard\n\nMozReview-Commit-ID: LaOwxXwhsHA'},
                               {'desc': u'Bug 1360626 - Create a blacklist for adaptive playback support. r=jolin, a=lizzard\n\nOn some devices / os combinations, enabling adaptive playback causes decoded frame unusable.\nIt may cause the decode frame to be black and white or return tiled frames.\nSo we should do the blacklist according to the report.\n\nMozReview-Commit-ID: j3PZXTtkXG'}]},
            8: {'changesets': [{'desc': u'Bug 1354038 - part2: [push-apk] taskgraph: Use rollout and deactivate dry-run on release r=aki a=bustage DONTBUILD\n\nMozReview-Commit-ID: 1f22BcAZkvp'}]},
            9: {'changesets': [{'desc': u'bug 1354038 - empty commit to force builds. a=release'}]},
            10: {'changesets': [{'desc': u'Bug 1337861 - [Fennec-Relpro] Enforce the presence of $MOZ_BUILD_DATE r=jlund a=release\n\nMozReview-Commit-ID: DzEeeYQjwLW'}]},
            11: {'changesets': [{'desc': u'Bug 1332731 - Follow-up to fix accessibility breakage. r=sebastian, a=lizzard\n\nFollow-up to fix breakage in accessibility caused by the bundle\nconversion. In particular, optString(foo) should have been converted to\ngetString(foo, "") because optString returns "" by default.\n\nAlso fix a small bug in Presentation.jsm where an array or null should\nbe used instead of a string.'}]},
            12: {'changesets': [{'desc': u'Bug 1355870 - Allow a system preference to determine distribution dir. r=nalexander, a=lizzard'}]},
            13: {'changesets': [{'desc': u'Bug 1354911 - Guard against null menu item names. r=sebastian, a=lizzard\n\nAddons may give us invalid menu item names; bail instead of crashing in\nsuch cases.'},
                                {'desc': u'Bug 1356563 - Remove chrome-document-loaded observer only after handling it. r=me, a=gchang\n\nOnly remove the "chrome-document-loaded" observer after handling it in\nnsAppShell. Otherwise we may never end up handling it.'}]},
            14: {'changesets': [{'desc': u'Bug 1352333 - remove autophone webrtc test manifests, r=dminor, a=test-only.'}]},
            15: {'changesets': [{'desc': u'Bug 1352333 - sync autophone webrtc test manifests with normal webrtc manifests, r=jmaher,dminor, a=test-only.'}]},
            16: {'changesets': [{'desc': u'No bug - Tagging f239279b709072490993b099832fa8c18f07713a with FENNEC_53_0_BUILD1, FENNEC_53_0_RELEASE a=release CLOSED TREE'}]},
            17: {'changesets': [{'desc': u'Automated checkin: version bump for fennec 53.0.1 release. DONTBUILD CLOSED TREE a=release'},
                                {'desc': u'Added FENNEC_53_0_1_RELEASE FENNEC_53_0_1_BUILD1 tag(s) for changeset f029d1a1324b. DONTBUILD CLOSED TREE a=release'}]},
            18: {'changesets': [{'desc': u"Backout Bug 1337861 (Enforce MOZ_BUILD_DATE) due to Bug 1360550. r=catlee a=catlee\n\nBug 1360550 resulted in the buildid the Linux builds had being different than the directory they were uploaded to. This had fallout affects for QA's firefox-ui tests and presumably anything using mozdownload.\n\nMozReview-Commit-ID: 8lMvLU0vGiS"}]},
            19: {'changesets': [{'desc': u'No bug, Automated blocklist update from host bld-linux64-spot-303 - a=blocklist-update'}]},
            20: {'changesets': [{'desc': u'Automatic version bump. CLOSED TREE NO BUG a=release'},
                                {'desc': u'No bug - Tagging 5cbf464688a47129c0ea36fe38f42f59926e4b2c with FENNEC_53_0_1_BUILD2, FENNEC_53_0_1_RELEASE a=release CLOSED TREE'}]}}
        bugs, backouts = get_bugs_in_changeset(changeset_data)

        assert bugs == {u'1356563', u'1348409', u'1341190', u'1360626', u'1332731', u'1328762',
                        u'1355870', u'1358089', u'1354911', u'1354038'}
        assert backouts == {u'1337861', u'1320072'}
