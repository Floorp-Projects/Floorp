# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import sqlite3 as lite


class Node(object):

    def __init__(self, graph, node_id):
        sql_result = graph.query_arg('SELECT id, dir, type, mtime, name \
            FROM node WHERE id=?', (node_id,)).fetchone()
        self.id, self.dir, self.type, self.mtime, self.name = sql_result
        children = graph.query_arg('SELECT to_id FROM \
            normal_link WHERE from_id=?', (self.id,)).fetchall()
        self.children = [graph.get_node(x) for (x,) in children]
        self.cmds = list(set(self.get_cmd_nodes()))
        self.path = self.get_path(graph) if self.type == 0 else ''
        self.cost = self.calculate_mtime()
        graph.add_node(self.id, self)

    @property
    def num_cmds(self):
        return len(self.cmds)

    def get_cmd_nodes(self):
        res = []
        if self.type == 1:
            res += [self]
        return res + [c for x in self.children for c in x.cmds]

    def get_cmd_ids(self):
        return [x.id for x in self.cmds]

    def get_path(self, graph):
        if self.dir == 1:
            return self.name
        parent = graph.get_node(self.dir)
        return os.path.join(parent.get_path(graph), self.name)

    def calculate_mtime(self):
        if self.type == 0:  # only files have meaningful costs
            return sum(x.mtime for x in self.cmds)
        else:
            return None


class Graph(object):

    def __init__(self, path=None, connect=None):
        self.connect = connect
        if path is not None:
            self.connect = lite.connect(path)
        elif self.connect is None:
            raise Exception
        if not self.table_check():
            print('\n Tup db does not have the necessary tables.')
            raise Exception
        self.node_dict = {}
        self.results = None

    def table_check(self):
        tables = [x[0] for x in self.query_arg('SELECT name \
            FROM sqlite_master WHERE type=?', ('table',)).fetchall()]
        return ('node' in tables and 'normal_link' in tables)

    def close(self):
        self.connect.close()

    def query_arg(self, q, arg):
        assert isinstance(arg, tuple)  # execute() requires tuple argument
        cursor = self.connect.cursor()
        cursor.execute(q, arg)
        return cursor

    def query(self, q):
        cursor = self.connect.cursor()
        cursor.execute(q)
        return cursor

    @property
    def nodes(self):
        return self.node_dict

    def add_node(self, k, v):
        self.node_dict[k] = v

    def get_id(self, filepath):
        nodeid = 1
        for part in filepath.split('/'):
            ret = self.query_arg('SELECT id FROM node \
                WHERE dir=? AND name=?', (nodeid, part)).fetchone()
            # fetchone should be ok bc dir and and name combo is unique
            if ret is None:
                print("\nCould not find id number for '%s'" % filepath)
                return None
            nodeid = ret[0]
        return nodeid

    def get_node(self, node_id):
        if node_id is not None:
            node = self.node_dict.get(node_id)
            if node is None:
                return Node(self, node_id)
            else:
                return node

    def file_summaries(self, files):
        for f in files:
            node = self.get_node(self.get_id(f))
            if node is not None:
                sec = node.cost / 1000.0
                m, s = sec / 60, sec % 60
                print("\n------ Summary for %s ------\
                      \nTotal Build Time (mm:ss) = %d:%d\nNum Downstream Commands = %d"
                      % (f, m, s, node.num_cmds))

    def populate(self):
        # make nodes for files with downstream commands
        files = self.query('SELECT id FROM node WHERE type=0 AND id in \
            (SELECT DISTINCT from_id FROM normal_link)').fetchall()
        res = []
        for (i,) in files:
            node = self.get_node(i)
            res.append((node.path, node.cost))
        self.results = res

    def get_cost_dict(self):
        if self.results is None:
            self.populate()
        return {k: v for k, v in self.results if v > 0}
