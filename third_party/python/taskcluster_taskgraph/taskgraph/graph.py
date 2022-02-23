# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import attr
import collections


@attr.s(frozen=True)
class Graph:
    """Generic representation of a directed acyclic graph with labeled edges
    connecting the nodes. Graph operations are implemented in a functional
    manner, so the data structure is immutable.

    It permits at most one edge of a given name between any set of nodes.  The
    graph is not checked for cycles, and methods may hang or otherwise fail if
    given a cyclic graph.

    The `nodes` and `edges` attributes may be accessed in a read-only fashion.
    The `nodes` attribute is a set of node names, while `edges` is a set of
    `(left, right, name)` tuples representing an edge named `name` going from
    node `left` to node `right`..
    """

    nodes = attr.ib(converter=frozenset)
    edges = attr.ib(converter=frozenset)

    def transitive_closure(self, nodes, reverse=False):
        """Return the transitive closure of <nodes>: the graph containing all
        specified nodes as well as any nodes reachable from them, and any
        intervening edges.

        If `reverse` is true, the "reachability" will be reversed and this
        will return the set of nodes that can reach the specified nodes.

        Example:

        .. code-block::

            a ------> b ------> c
                      |
                      `-------> d

        transitive_closure([b]).nodes == set([a, b])
        transitive_closure([c]).nodes == set([c, b, a])
        transitive_closure([c], reverse=True).nodes == set([c])
        transitive_closure([b], reverse=True).nodes == set([b, c, d])
        """
        assert isinstance(nodes, set)
        if not (nodes <= self.nodes):
            raise Exception(
                f"Unknown nodes in transitive closure: {nodes - self.nodes}"
            )

        # generate a new graph by expanding along edges until reaching a fixed
        # point
        new_nodes, new_edges = nodes, set()
        nodes, edges = set(), set()
        while (new_nodes, new_edges) != (nodes, edges):
            nodes, edges = new_nodes, new_edges
            add_edges = {
                (left, right, name)
                for (left, right, name) in self.edges
                if (right if reverse else left) in nodes
            }
            add_nodes = {(left if reverse else right) for (left, right, _) in add_edges}
            new_nodes = nodes | add_nodes
            new_edges = edges | add_edges
        return Graph(new_nodes, new_edges)

    def _visit(self, reverse):
        queue = collections.deque(sorted(self.nodes))
        links_by_node = self.reverse_links_dict() if reverse else self.links_dict()
        seen = set()
        while queue:
            node = queue.popleft()
            if node in seen:
                continue
            links = links_by_node[node]
            if all((n in seen) for n in links):
                seen.add(node)
                yield node
            else:
                queue.extend(n for n in links if n not in seen)
                queue.append(node)

    def visit_postorder(self):
        """
        Generate a sequence of nodes in postorder, such that every node is
        visited *after* any nodes it links to.

        Behavior is undefined (read: it will hang) if the graph contains a
        cycle.
        """
        return self._visit(False)

    def visit_preorder(self):
        """
        Like visit_postorder, but in reverse: evrey node is visited *before*
        any nodes it links to.
        """
        return self._visit(True)

    def links_dict(self):
        """
        Return a dictionary mapping each node to a set of the nodes it links to
        (omitting edge names)
        """
        links = collections.defaultdict(set)
        for left, right, _ in self.edges:
            links[left].add(right)
        return links

    def named_links_dict(self):
        """
        Return a two-level dictionary mapping each node to a dictionary mapping
        edge names to labels.
        """
        links = collections.defaultdict(dict)
        for left, right, name in self.edges:
            links[left][name] = right
        return links

    def reverse_links_dict(self):
        """
        Return a dictionary mapping each node to a set of the nodes linking to
        it (omitting edge names)
        """
        links = collections.defaultdict(set)
        for left, right, _ in self.edges:
            links[right].add(left)
        return links
