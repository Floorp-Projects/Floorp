# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import unittest

from gecko_taskgraph.graph import Graph
from mozunit import main


class TestGraph(unittest.TestCase):

    tree = Graph(
        {"a", "b", "c", "d", "e", "f", "g"},
        {
            ("a", "b", "L"),
            ("a", "c", "L"),
            ("b", "d", "K"),
            ("b", "e", "K"),
            ("c", "f", "N"),
            ("c", "g", "N"),
        },
    )

    linear = Graph(
        {"1", "2", "3", "4"},
        {
            ("1", "2", "L"),
            ("2", "3", "L"),
            ("3", "4", "L"),
        },
    )

    diamonds = Graph(
        {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"},
        {
            tuple(x)
            for x in "AFL ADL BDL BEL CEL CHL DFL DGL EGL EHL FIL GIL GJL HJL".split()
        },
    )

    multi_edges = Graph(
        {"1", "2", "3", "4"},
        {
            ("2", "1", "red"),
            ("2", "1", "blue"),
            ("3", "1", "red"),
            ("3", "2", "blue"),
            ("3", "2", "green"),
            ("4", "3", "green"),
        },
    )

    disjoint = Graph(
        {"1", "2", "3", "4", "α", "β", "γ"},
        {
            ("2", "1", "red"),
            ("3", "1", "red"),
            ("3", "2", "green"),
            ("4", "3", "green"),
            ("α", "β", "πράσινο"),
            ("β", "γ", "κόκκινο"),
            ("α", "γ", "μπλε"),
        },
    )

    def test_transitive_closure_empty(self):
        "transitive closure of an empty set is an empty graph"
        g = Graph({"a", "b", "c"}, {("a", "b", "L"), ("a", "c", "L")})
        self.assertEqual(g.transitive_closure(set()), Graph(set(), set()))

    def test_transitive_closure_disjoint(self):
        "transitive closure of a disjoint set is a subset"
        g = Graph({"a", "b", "c"}, set())
        self.assertEqual(g.transitive_closure({"a", "c"}), Graph({"a", "c"}, set()))

    def test_transitive_closure_trees(self):
        "transitive closure of a tree, at two non-root nodes, is the two subtrees"
        self.assertEqual(
            self.tree.transitive_closure({"b", "c"}),
            Graph(
                {"b", "c", "d", "e", "f", "g"},
                {
                    ("b", "d", "K"),
                    ("b", "e", "K"),
                    ("c", "f", "N"),
                    ("c", "g", "N"),
                },
            ),
        )

    def test_transitive_closure_multi_edges(self):
        "transitive closure of a tree with multiple edges between nodes keeps those edges"
        self.assertEqual(
            self.multi_edges.transitive_closure({"3"}),
            Graph(
                {"1", "2", "3"},
                {
                    ("2", "1", "red"),
                    ("2", "1", "blue"),
                    ("3", "1", "red"),
                    ("3", "2", "blue"),
                    ("3", "2", "green"),
                },
            ),
        )

    def test_transitive_closure_disjoint_edges(self):
        "transitive closure of a disjoint graph keeps those edges"
        self.assertEqual(
            self.disjoint.transitive_closure({"3", "β"}),
            Graph(
                {"1", "2", "3", "β", "γ"},
                {
                    ("2", "1", "red"),
                    ("3", "1", "red"),
                    ("3", "2", "green"),
                    ("β", "γ", "κόκκινο"),
                },
            ),
        )

    def test_transitive_closure_linear(self):
        "transitive closure of a linear graph includes all nodes in the line"
        self.assertEqual(self.linear.transitive_closure({"1"}), self.linear)

    def test_visit_postorder_empty(self):
        "postorder visit of an empty graph is empty"
        self.assertEqual(list(Graph(set(), set()).visit_postorder()), [])

    def assert_postorder(self, seq, all_nodes):
        seen = set()
        for e in seq:
            for l, r, n in self.tree.edges:
                if l == e:
                    self.assertTrue(r in seen)
            seen.add(e)
        self.assertEqual(seen, all_nodes)

    def test_visit_postorder_tree(self):
        "postorder visit of a tree satisfies invariant"
        self.assert_postorder(self.tree.visit_postorder(), self.tree.nodes)

    def test_visit_postorder_diamonds(self):
        "postorder visit of a graph full of diamonds satisfies invariant"
        self.assert_postorder(self.diamonds.visit_postorder(), self.diamonds.nodes)

    def test_visit_postorder_multi_edges(self):
        "postorder visit of a graph with duplicate edges satisfies invariant"
        self.assert_postorder(
            self.multi_edges.visit_postorder(), self.multi_edges.nodes
        )

    def test_visit_postorder_disjoint(self):
        "postorder visit of a disjoint graph satisfies invariant"
        self.assert_postorder(self.disjoint.visit_postorder(), self.disjoint.nodes)

    def assert_preorder(self, seq, all_nodes):
        seen = set()
        for e in seq:
            for l, r, n in self.tree.edges:
                if r == e:
                    self.assertTrue(l in seen)
            seen.add(e)
        self.assertEqual(seen, all_nodes)

    def test_visit_preorder_tree(self):
        "preorder visit of a tree satisfies invariant"
        self.assert_preorder(self.tree.visit_preorder(), self.tree.nodes)

    def test_visit_preorder_diamonds(self):
        "preorder visit of a graph full of diamonds satisfies invariant"
        self.assert_preorder(self.diamonds.visit_preorder(), self.diamonds.nodes)

    def test_visit_preorder_multi_edges(self):
        "preorder visit of a graph with duplicate edges satisfies invariant"
        self.assert_preorder(self.multi_edges.visit_preorder(), self.multi_edges.nodes)

    def test_visit_preorder_disjoint(self):
        "preorder visit of a disjoint graph satisfies invariant"
        self.assert_preorder(self.disjoint.visit_preorder(), self.disjoint.nodes)

    def test_links_dict(self):
        "link dict for a graph with multiple edges is correct"
        self.assertEqual(
            self.multi_edges.links_dict(),
            {
                "2": {"1"},
                "3": {"1", "2"},
                "4": {"3"},
            },
        )

    def test_named_links_dict(self):
        "named link dict for a graph with multiple edges is correct"
        self.assertEqual(
            self.multi_edges.named_links_dict(),
            {
                "2": dict(red="1", blue="1"),
                "3": dict(red="1", blue="2", green="2"),
                "4": dict(green="3"),
            },
        )

    def test_reverse_links_dict(self):
        "reverse link dict for a graph with multiple edges is correct"
        self.assertEqual(
            self.multi_edges.reverse_links_dict(),
            {
                "1": {"2", "3"},
                "2": {"3"},
                "3": {"4"},
            },
        )


if __name__ == "__main__":
    main()
