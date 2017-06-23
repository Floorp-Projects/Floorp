# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import unittest
import shutil
from StringIO import StringIO
import json

from mozbuild.codecoverage import lcov_rewriter
import buildconfig
import mozunit

here = os.path.dirname(__file__)

basic_file = """TN:Compartment_5f7f5c30251800
SF:resource://gre/modules/osfile.jsm
FN:1,top-level
FNDA:1,top-level
FNF:1
FNH:1
BRDA:9,0,61,1
BRF:1
BRH:1
DA:9,1
DA:24,1
LF:2
LH:2
end_of_record
"""

# These line numbers are (synthetically) sorted.
multiple_records = """SF:resource://gre/modules/workers/require.js
FN:1,top-level
FN:80,.get
FN:95,require
FNDA:1,top-level
FNF:3
FNH:1
BRDA:46,0,16,-
BRDA:135,225,446,-
BRF:2
BRH:0
DA:43,1
DA:46,1
DA:152,0
DA:163,1
LF:4
LH:3
end_of_record
SF:resource://gre/modules/osfile/osfile_async_worker.js
FN:12,top-level
FN:30,worker.dispatch
FN:34,worker.postMessage
FN:387,do_close
FN:392,exists
FN:394,do_exists
FN:400,unixSymLink
FNDA:1,do_exists
FNDA:1,exists
FNDA:1,top-level
FNDA:594,worker.dispatch
FNF:7
FNH:4
BRDA:6,0,30,1
BRDA:365,0,103,-
BRF:2
BRH:1
DA:6,1
DA:7,0
DA:12,1
DA:18,1
DA:19,1
DA:20,1
DA:23,1
DA:25,1
DA:401,0
DA:407,1
LF:10
LH:8
end_of_record
"""

fn_with_multiple_commas = """TN:Compartment_5f7f5c30251800
SF:resource://gre/modules/osfile.jsm
FN:1,function,name,with,commas
FNDA:1,function,name,with,commas
FNF:1
FNH:1
BRDA:9,0,61,1
BRF:1
BRH:1
DA:9,1
DA:24,1
LF:2
LH:2
end_of_record
"""

class TestLcovParser(unittest.TestCase):

    def get_lcov(self, lcov_string):
        fh = StringIO(lcov_string)
        return lcov_rewriter.LcovFile(fh)

    def parser_roundtrip(self, lcov_string, resummarize=False):
        file_obj = self.get_lcov(lcov_string)
        if resummarize:
            for r in file_obj.records:
                r.resummarize()
        out = StringIO()
        file_obj.print_file(out)
        return out.getvalue()

    def test_basic_parse(self):
        output = self.parser_roundtrip(basic_file)
        self.assertEqual(basic_file, output)

        output = self.parser_roundtrip(multiple_records)
        self.assertEqual(multiple_records, output)

    def test_resummarize(self):
        output = self.parser_roundtrip(basic_file, True)
        self.assertEqual(basic_file, output)

        output = self.parser_roundtrip(multiple_records, True)
        self.assertEqual(multiple_records, output)

    def test_multiple_commas(self):
        output = self.parser_roundtrip(fn_with_multiple_commas, True)
        self.assertEqual(fn_with_multiple_commas, output)

multiple_included_files = """//@line 1 "foo.js"
bazfoobar
//@line 2 "bar.js"
@foo@
//@line 3 "foo.js"
bazbarfoo
//@line 2 "bar.js"
foobarbaz
//@line 3 "test.js"
barfoobaz
//@line 1 "baz.js"
baz
//@line 6 "f.js"
fin
"""

class TestLineRemapping(unittest.TestCase):
    def setUp(self):
        chrome_map_file = os.path.join(buildconfig.topobjdir, 'chrome-map.json')
        self._old_chrome_info_file = None
        if os.path.isfile(chrome_map_file):
            backup_file = os.path.join(buildconfig.topobjdir, 'chrome-map-backup.json')
            self._old_chrome_info_file = backup_file
            self._chrome_map_file = chrome_map_file
            shutil.move(chrome_map_file, backup_file)

        empty_chrome_info = [
            {}, {}, {},
        ]
        with open(chrome_map_file, 'w') as fh:
            json.dump(empty_chrome_info, fh)

        self.lcov_rewriter = lcov_rewriter.LcovFileRewriter('', '', [])
        self.pp_rewriter = self.lcov_rewriter.pp_rewriter

    def tearDown(self):
        if self._old_chrome_info_file:
            shutil.move(self._old_chrome_info_file, self._chrome_map_file)

    def get_lcov_file(self, name):
        fpath = os.path.join(here, name)
        with open(fpath) as fh:
            lcov_file = lcov_rewriter.LcovFile(fh)
        return lcov_file

    def test_map_multiple_included(self):
        self.pp_rewriter.populate_pp_info(StringIO(multiple_included_files),
                                          '')
        actual = self.pp_rewriter.pp_info['']
        expected = {
            (2, 3): ('foo.js', 1),
            (4, 5): ('bar.js', 2),
            (6, 7): ('foo.js', 3),
            (8, 9): ('bar.js', 2),
            (10, 11): ('test.js', 3),
            (12, 13): ('baz.js', 1),
            (14, 15): ('f.js', 6),
        }

        self.assertEqual(actual, expected)

    def test_remap_lcov(self):
        pp_remap = {
            (1941, 2158): ('dropPreview.js', 6),
            (2159, 2331): ('updater.js', 6),
            (2584, 2674): ('intro.js', 6),
            (2332, 2443): ('undo.js', 6),
            (864, 985): ('cells.js', 6),
            (2444, 2454): ('search.js', 6),
            (1567, 1712): ('drop.js', 6),
            (2455, 2583): ('customize.js', 6),
            (1713, 1940): ('dropTargetShim.js', 6),
            (1402, 1548): ('drag.js', 6),
            (1549, 1566): ('dragDataHelper.js', 6),
            (453, 602): ('page.js', 141),
            (2675, 2678): ('newTab.js', 70),
            (56, 321): ('transformations.js', 6),
            (603, 863): ('grid.js', 6),
            (322, 452): ('page.js', 6),
            (986, 1401): ('sites.js', 6)
        }

        lcov_file = self.get_lcov_file('sample_lcov.info')

        self.assertEqual(len(lcov_file.records), 1)

        # This summarization changes values due multiple reports per line coming
        # from the JS engine (bug 1198356).
        for r in lcov_file.records:
            r.resummarize()
            original_line_count = r.line_count
            original_covered_line_count = r.covered_line_count
            original_function_count = r.function_count
            original_covered_function_count = r.covered_function_count

        self.pp_rewriter.pp_info['lcov_test_newTab.js'] = pp_remap
        additions = []
        for r in lcov_file.records:
            additions += self.pp_rewriter.rewrite_record(r)
        lcov_file.records = lcov_file.records + additions
        self.assertEqual(len(lcov_file.records), 17)

        # Lines/functions are only "moved" between records, not duplicated or omited.
        self.assertEqual(original_line_count,
                         sum(r.line_count for r in lcov_file.records))
        self.assertEqual(original_covered_line_count,
                         sum(r.covered_line_count for r in lcov_file.records))
        self.assertEqual(original_function_count,
                         sum(r.function_count for r in lcov_file.records))
        self.assertEqual(original_covered_function_count,
                         sum(r.covered_function_count for r in lcov_file.records))

class TestUrlFinder(unittest.TestCase):
    def setUp(self):
        chrome_map_file = os.path.join(buildconfig.topobjdir, 'chrome-map.json')
        self._old_chrome_info_file = None
        if os.path.isfile(chrome_map_file):
            backup_file = os.path.join(buildconfig.topobjdir, 'chrome-map-backup.json')
            self._old_chrome_info_file = backup_file
            self._chrome_map_file = chrome_map_file
            shutil.move(chrome_map_file, backup_file)

        empty_chrome_info = [
            {},
            {},
            {
                os.path.join('dist', 'bin', 'components', 'MainProcessSingleton.js'): [
                    'path1',
                    False
                ],
                os.path.join('dist', 'bin', 'browser', 'components', 'nsSessionStartup.js'): [
                    'path2',
                    False
                ],
                os.path.join('dist', 'bin', 'browser', 'features', 'e10srollout@mozilla.org', 'bootstrap.js'): [
                    'path3',
                    False
                ],
                os.path.join('dist', 'bin', 'browser', 'features', 'firefox@getpocket.com', 'bootstrap.js'): [
                    'path4',
                    False
                ],
                os.path.join('dist', 'xpi-stage', 'workerbootstrap', 'bootstrap.js'): [
                    'path5',
                    False
                ]
            },
        ]
        with open(chrome_map_file, 'w') as fh:
            json.dump(empty_chrome_info, fh)

    def tearDown(self):
        if self._old_chrome_info_file:
            shutil.move(self._old_chrome_info_file, self._chrome_map_file)

    def test_jar_paths(self):
        app_name = buildconfig.substs.get('MOZ_APP_NAME')
        omnijar_name = buildconfig.substs.get('OMNIJAR_NAME')

        paths = [
            ('jar:file:///home/worker/workspace/build/application/' + app_name + '/' + omnijar_name + '!/components/MainProcessSingleton.js', 'path1'),
            ('jar:file:///home/worker/workspace/build/application/' + app_name + '/browser/' + omnijar_name + '!/components/nsSessionStartup.js', 'path2'),
            ('jar:file:///home/worker/workspace/build/application/' + app_name + '/browser/features/e10srollout@mozilla.org.xpi!/bootstrap.js', 'path3'),
            ('jar:file:///home/worker/workspace/build/application/' + app_name + '/browser/features/firefox@getpocket.com.xpi!/bootstrap.js', 'path4'),
            ('jar:file:///tmp/tmpMdo5gV.mozrunner/extensions/workerbootstrap-test@mozilla.org.xpi!/bootstrap.js', 'path5'),
        ]

        url_finder = lcov_rewriter.UrlFinder('', '', [])
        for path, expected in paths:
            self.assertEqual(url_finder.rewrite_url(path)[0], expected)

if __name__ == '__main__':
    mozunit.main()
