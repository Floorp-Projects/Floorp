# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import unittest
import sqlite3 as lite
import mozunit

from mozbuild.analyze.graph import Graph

CREATE_NODE = """CREATE TABLE node
        (id integer PRIMARY KEY NOT NULL,
        dir integer NOT NULL,
        type integer NOT NULL,
        mtime integer NOT NULL,
        name varchar(4096) NOT NULL,
        unique(dir, name));"""

CREATE_NORMAL_LINK = """CREATE TABLE normal_link
        (from_id integer,
        to_id integer, unique(from_id, to_id));"""

NODE_DATA = [(1, 0 ,2, -1, '.'),
        (2, 100, 0, 1, 'Base64.cpp'),
        (3, 200, 0, 1, 'nsArray.cpp'),
        (4, 100, 0, 1, 'nsWildCard.h'),
        (5, -1, 1, 9426, 'CDD Unified_cpp_xpcom_io0.cpp'),
        (6, -1, 1, 5921, 'CXX Unified_cpp_xpcom_ds0.cpp'),
        (7, -1, 1, 11077, 'CXX /builds/worker/workspace/build/src/dom/\
            plugins/base/snNPAPIPlugin.cpp'),
        (8, -1, 1, 7677, 'CXX Unified_cpp_xpcom_io1.cpp'),
        (9, -1, 1, 8672, 'CXX Unified_cpp_modules_libjar0.cpp'),
        (10, -1, 4, 1, 'Unified_cpp_xpcom_io0.o'),
        (11, -1, 4, 1, 'Unified_cpp_xpcom_dso.o'),
        (12, -1, 4, 1, 'nsNPAPIPlugin.o'),
        (13, -1, 4, 1, 'Unified_cpp_xpcom_io1.o'),
        (14, -1, 4, 1, 'Unified_cpp_modules_libjar0.o'),
        (15, -1, 1, 52975, 'LINK libxul.so'),
        (16, -1, 4, 1, 'libxul.so'),
        (17, -1, 1, 180, 'LINK libtestcrasher.so'),
        (18, -1, 1, 944, 'python /builds/worker/workspace/build/src/toolkit/\
            library/dependentlibs.py:gen_list -> [dependentlibs.list, \
            dependentlibs.list.gtest, dependentlibs.list.pp]'),
        (19, -1, 1, 348, 'LINK ../../dist/bin/plugin-container'),
        (20, -1, 1, 342, 'LINK ../../../dist/bin/xpcshell'),
        (21, -1, 4, 1, 'libtestcrasher.so'),
        (22, -1, 4, 1, 'dependentlibs.list'),
        (23, -1, 4, 1, 'dependentlibs.list.gtest'),
        (24, -1, 4, 1, 'dependentlibs.list.pp'),
        (25, -1, 4, 1, 'plugin-container'),
        (26, -1, 4, 1, 'xpcshell'),
        (27, -1, 6, 1, '<shlibs>'),
        (28, 1, 0, 1, 'dummy node'),
        (100, 300, 2, -1, 'io'),
        (200, 300, 2, -1, 'ds'),
        (300, 1, 2, -1, 'xpcom')]

NORMAL_LINK_DATA = [(2, 5), (3, 6), (4, 7), (4, 8), (4, 9), (5, 10), (6, 11),
                    (7, 12), (8, 13), (9, 14), (10, 15), (11, 15), (12, 15),
                    (13, 15), (14, 15), (15, 16), (15, 27), (16, 17), (16, 18),
                    (16, 19), (16, 20), (17, 21), (17, 27), (18, 22), (18, 23),
                    (18, 24), (19, 25), (20, 26), (21, 27)]

PATH_TO_TEST_DB = ':memory:'

class TestGraph(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.connect = lite.connect(PATH_TO_TEST_DB)
        cls.connect.text_factory = str
        cls.cursor = cls.connect.cursor()
        # create tables in memory
        cls.cursor.execute(CREATE_NODE)
        cls.cursor.execute(CREATE_NORMAL_LINK)
        # insert data to tables
        for d in NODE_DATA:
            cls.cursor.execute("""INSERT INTO node \
                (id, dir, type, mtime, name) values(?, ?, ?, ?, ?)""", d)
        for d in NORMAL_LINK_DATA:
            cls.cursor.execute("""INSERT INTO normal_link \
                (from_id, to_id) values(?, ?)""", d)
        cls.connect.commit()

    @classmethod
    def tearDownClass(cls):
        cls.connect.close()

    def test_graph_cmds(self):
        g = Graph(connect=self.connect)
        libxul = [15, 17, 18, 19, 20]
        # no commands
        self.assertEqual(len(g.get_node(27).cmds), 0)
        self.assertEqual(len(g.get_node(21).cmds), 0)
        self.assertEqual(len(g.get_node(28).cmds), 0)
        # one immediate command child
        self.assertItemsEqual(g.get_node(2).get_cmd_ids(),[5] + libxul)
        self.assertItemsEqual(g.get_node(3).get_cmd_ids(),[6] + libxul)
        # multiple immediate command children
        self.assertItemsEqual(g.get_node(4).get_cmd_ids(),[7, 8, 9] + libxul)
        # node is not a file or command
        self.assertItemsEqual(g.get_node(16).get_cmd_ids(), libxul[1:])
        self.assertItemsEqual(g.get_node(11).get_cmd_ids(), libxul)
        # node is the command
        self.assertItemsEqual(g.get_node(19).get_cmd_ids(), [19])
        self.assertItemsEqual(g.get_node(7).get_cmd_ids(), [7] + libxul)
        self.assertItemsEqual(g.get_node(15).get_cmd_ids(), libxul)

    def test_mtime_calc(self):
        g = Graph(connect=self.connect)
        # one immediate child
        self.assertEqual(g.get_node(2).cost, 64215)
        self.assertEqual(g.get_node(3).cost, 60710)
        # multiple immediate children
        self.assertEqual(g.get_node(4).cost, 82215)
        # no children
        self.assertEqual(g.get_node(23).cost, None)
        # not a file
        self.assertEqual(g.get_node(15).cost, None)
        # file with no commands
        self.assertEqual(g.get_node(28).cost, 0)

    def test_path(self):
        g = Graph(connect=self.connect)
        # node is not a file
        self.assertEqual(g.get_node(15).path, '')
        self.assertEqual(g.get_node(16).path, '')
        self.assertEqual(g.get_node(27).path, '')
        # node is a file
        self.assertEqual(g.get_node(2).path, 'xpcom/io/Base64.cpp')
        self.assertEqual(g.get_node(3).path, 'xpcom/ds/nsArray.cpp')
        self.assertEqual(g.get_node(4).path, 'xpcom/io/nsWildCard.h')
        self.assertEqual(g.get_node(28).path, 'dummy node')

if __name__ == '__main__':
  mozunit.main()