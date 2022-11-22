# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Incremental algorithm for creating a "deterministic acyclic finite state
automaton" (DAFSA). At the time of writing this algorithm, there was existing logic
that depended on a different format for the DAFSA, so this contains convenience
functions for converting to a compatible structure. This legacy format is defined
in make_dafsa.py.
"""

from typing import Callable, Dict, List, Optional


class Node:
    children: Dict[str, "Node"]
    parents: Dict[str, List["Node"]]
    character: str
    is_root_node: bool
    is_end_node: bool

    def __init__(self, character, is_root_node=False, is_end_node=False):
        self.children = {}
        self.parents = {}
        self.character = character
        self.is_root_node = is_root_node
        self.is_end_node = is_end_node

    def __str__(self):
        """Produce a helpful string representation of this node.

        This is expected to only be used for debugging.
        The produced output is:

        "c[def.] <123>"
         ^ ^      ^
         | |      Internal python ID of the node (used for de-duping)
         | |
         | One possible path through the tree to the end
         |
         Current node character
        """

        if self.is_root_node:
            return "<root>"
        elif self.is_end_node:
            return "<end>"

        first_potential_match = ""
        node = self
        while node.children:
            first_character = next(iter(node.children))
            if first_character:
                first_potential_match += first_character
            node = node.children[first_character]

        return "%s[%s] <%d>" % (self.character, first_potential_match, id(self))

    def add_child(self, child):
        self.children[child.character] = child
        child.parents.setdefault(self.character, [])
        child.parents[self.character].append(self)

    def remove(self):
        # remove() must only be called when this node has only a single parent, and that
        # parent doesn't need this child anymore.
        # The caller is expected to have performed this validation.
        # (placing asserts here add a non-trivial performance hit)

        # There's only a single parent, so only one list should be in the "parents" map
        parent_list = next(iter(self.parents.values()))
        self.remove_parent(parent_list[0])
        for child in list(self.children.values()):
            child.remove_parent(self)

    def remove_parent(self, parent_node: "Node"):
        parent_node.children.pop(self.character)
        parents_for_character = self.parents[parent_node.character]
        parents_for_character.remove(parent_node)
        if not parents_for_character:
            self.parents.pop(parent_node.character)

    def copy_fork_node(self, fork_node: "Node", child_to_avoid: Optional["Node"]):
        """Shallow-copy a node's children.

        When adding a new word, sometimes previously-joined suffixes aren't perfect
        matches any more. When this happens, some nodes need to be "copied" out.
        For all non-end nodes, there's a child to avoid in the shallow-copy.
        """

        for child in fork_node.children.values():
            if child is not child_to_avoid:
                self.add_child(child)

    def is_fork(self):
        """Check if this node has multiple parents"""

        if len(self.parents) == 0:
            return False

        if len(self.parents) > 1:
            return True

        return len(next(iter(self.parents.values()))) > 1

    def is_replacement_for_prefix_end_node(self, old: "Node"):
        """Check if this node is a valid replacement for an old end node.

        A node is a valid replacement if it maintains all existing child paths while
        adding the new child path needed for the new word.

        Args:
            old: node being replaced

        Returns: True if this node is a valid replacement node.
        """

        if len(self.children) != len(old.children) + 1:
            return False

        for character, other_node in old.children.items():
            this_node = self.children.get(character)
            if other_node is not this_node:
                return False

        return True

    def is_replacement_for_prefix_node(self, old: "Node"):
        """Check if this node is a valid replacement for a non-end node.

        A node is a valid replacement if it:
        * Has one new child that the old node doesn't
        * Is missing a child that the old node has
        * Shares all other children

        Returns: True if this node is a valid replacement node.
        """

        if len(self.children) != len(old.children):
            return False

        found_extra_child = False

        for character, other_node in old.children.items():
            this_node = self.children.get(character)
            if other_node is not this_node:
                if found_extra_child:
                    # Found two children in the old node that aren't in the new one,
                    # this isn't a valid replacement
                    return False
                else:
                    found_extra_child = True

        return found_extra_child


class SuffixCursor:
    index: int  # Current position of the cursor within the DAFSA.
    node: Node

    def __init__(self, index, node):
        self.index = index
        self.node = node

    def _query(self, character: str, check: Callable[[Node], bool]):
        for node in self.node.parents.get(character, []):
            if check(node):
                self.index -= 1
                self.node = node
                return True
        return False

    def find_single_child(self, character):
        """Find the next matching suffix node that has a single child.

        Return True if such a node is found."""
        return self._query(character, lambda node: len(node.children) == 1)

    def find_end_of_prefix_replacement(self, end_of_prefix: Node):
        """Find the next matching suffix node that replaces the old prefix-end node.

        Return True if such a node is found."""
        return self._query(
            end_of_prefix.character,
            lambda node: node.is_replacement_for_prefix_end_node(end_of_prefix),
        )

    def find_inside_of_prefix_replacement(self, prefix_node: Node):
        """Find the next matching suffix node that replaces a node within the prefix.

        Return True if such a node is found."""
        return self._query(
            prefix_node.character,
            lambda node: node.is_replacement_for_prefix_node(prefix_node),
        )


class DafsaAppendStateMachine:
    """State machine for adding a word to a Dafsa.

    Each state returns a function reference to the "next state". States should be
    invoked until "None" is returned, in which case the new word has been appended.

    The prefix and suffix indexes are placed according to the currently-known valid
    value (not the next value being investigated). Additionally, they are 0-indexed
    against the root node (which sits behind the beginning of the string).

    Let's imagine we're at the following state when adding, for example, the
    word "mozilla.org":

        mozilla.org
       ^    ^    ^ ^
       |    |    | |
      /     |    | \
    [root]  |    |  [end] node
    node    |    \
            |     suffix
            \
             prefix

    In this state, the furthest prefix match we could find was:
        [root] - m - o - z - i - l
    The index of the prefix match is "5".

    Additionally, we've been looking for suffix nodes, and we've already found:
        r - g - [end]
    The current suffix index is "10".
    The next suffix node we'll attempt to find is at index "9".
    """

    root_node: Node
    prefix_index: int
    suffix_cursor: SuffixCursor
    stack: List[Node]
    word: str
    suffix_overlaps_prefix: bool
    first_fork_index: Optional[int]
    _state: Callable

    def __init__(self, word, root_node, end_node):
        self.root_node = root_node
        self.prefix_index = 0
        self.suffix_cursor = SuffixCursor(len(word) + 1, end_node)
        self.stack = [root_node]
        self.word = word
        self.suffix_overlaps_prefix = False
        self.first_fork_index = None
        self._state = self._find_prefix

    def run(self):
        """Run this state machine to completion, adding the new word."""
        while self._state is not None:
            self._state = self._state()

    def _find_prefix(self):
        """Find the longest existing prefix that matches the current word."""
        prefix_node = self.root_node
        while self.prefix_index < len(self.word):
            next_character = self.word[self.prefix_index]
            next_node = prefix_node.children.get(next_character)
            if not next_node:
                # We're finished finding the prefix, let's find the longest suffix
                # match now.
                return self._find_suffix_nodes_after_prefix

            self.prefix_index += 1
            prefix_node = next_node
            self.stack.append(next_node)

            if not self.first_fork_index and next_node.is_fork():
                self.first_fork_index = self.prefix_index

        # Deja vu, we've appended this string before. Since this string has
        # already been appended, we don't have to do anything.
        return None

    def _find_suffix_nodes_after_prefix(self):
        """Find the chain of suffix nodes for characters after the prefix."""
        while self.suffix_cursor.index - 1 > self.prefix_index:
            # To fetch the next character, we need to subtract two from the current
            # suffix index. This is because:
            # * The next suffix node is 1 node before our current node (subtract 1)
            # * The suffix index includes the root node before the beginning of the
            #   string - it's like the string is 1-indexed (subtract 1 again).
            next_character = self.word[self.suffix_cursor.index - 2]
            if not self.suffix_cursor.find_single_child(next_character):
                return self._add_new_nodes

            if self.suffix_cursor.node is self.stack[-1]:
                # The suffix match is overlapping with the prefix! This can happen in
                # cases like:
                # * "ab"
                # * "abb"
                # The suffix cursor is at the same node as the prefix match, but they're
                # at different positions in the word.
                #
                # [root] - a - b - [end]
                #              ^
                #             / \
                #            /   \
                #      prefix     suffix
                #            \    /
                #             \  /
                #              VV
                #            "abb"
                if not self.first_fork_index:
                    # There hasn't been a fork, so our prefix isn't shared. So, we
                    # can mark this node as a fork, since the repetition means
                    # that there's two paths that are now using this node
                    self.first_fork_index = self.prefix_index
                    return self._add_new_nodes

                # Removes the link between the unique part of the prefix and the
                # shared part of the prefix.
                self.stack[self.first_fork_index].remove_parent(
                    self.stack[self.first_fork_index - 1]
                )
                self.suffix_overlaps_prefix = True

        if self.first_fork_index is None:
            return self._find_next_suffix_nodes
        elif self.suffix_cursor.index - 1 == self.first_fork_index:
            return self._find_next_suffix_node_at_prefix_end_at_fork
        else:
            return self._find_next_suffix_node_at_prefix_end_after_fork

    def _find_next_suffix_node_at_prefix_end_at_fork(self):
        """Find the next suffix node that replaces the end of the prefix.

        In this state, the prefix_end node is the same as the first fork node.
        Therefore, if a match can be found, the old prefix node can't be entirely
        deleted since it's used elsewhere. Instead, just the link between our
        unique prefix and the end of the fork is removed.
        """
        existing_node = self.stack[self.prefix_index]
        if not self.suffix_cursor.find_end_of_prefix_replacement(existing_node):
            return self._add_new_nodes

        self.prefix_index -= 1
        self.first_fork_index = None

        if not self.suffix_overlaps_prefix:
            existing_node.remove_parent(self.stack[self.prefix_index])
        else:
            # When the suffix overlaps the prefix, the old "parent link" was removed
            # earlier in the "find_suffix_nodes_after_prefix" step.
            self.suffix_overlaps_prefix = False

        return self._find_next_suffix_nodes

    def _find_next_suffix_node_at_prefix_end_after_fork(self):
        """Find the next suffix node that replaces the end of the prefix.

        In this state, the prefix_end node is after the first fork node.
        Therefore, even if a match is found, we don't want to modify the replaced
        prefix node since an unrelated word chain uses it.
        """
        existing_node = self.stack[self.prefix_index]
        if not self.suffix_cursor.find_end_of_prefix_replacement(existing_node):
            return self._add_new_nodes

        self.prefix_index -= 1
        if self.prefix_index == self.first_fork_index:
            return self._find_next_suffix_node_within_prefix_at_fork
        else:
            return self._find_next_suffix_nodes_within_prefix_after_fork

    def _find_next_suffix_node_within_prefix_at_fork(self):
        """Find the next suffix node within a prefix.

        In this state, we've already worked our way back and found nodes in the suffix
        to replace prefix nodes after the fork node. We have now reached the fork node,
        and if we find a replacement for it, then we can remove the link between it
        and our then-unique prefix and clear the fork status.
        """
        existing_node = self.stack[self.prefix_index]
        if not self.suffix_cursor.find_inside_of_prefix_replacement(existing_node):
            return self._add_new_nodes

        self.prefix_index -= 1
        self.first_fork_index = None

        if not self.suffix_overlaps_prefix:
            existing_node.remove_parent(self.stack[self.prefix_index])
        else:
            # When the suffix overlaps the prefix, the old "parent link" was removed
            # earlier in the "find_suffix_nodes_after_prefix" step.
            self.suffix_overlaps_prefix = False

        return self._find_next_suffix_nodes

    def _find_next_suffix_nodes_within_prefix_after_fork(self):
        """Find the next suffix nodes within a prefix.

        Finds suffix nodes to replace prefix nodes, but doesn't modify the prefix
        nodes since they're after a fork (so, we're sharing prefix nodes with
        other words and can't modify them).
        """
        while True:
            existing_node = self.stack[self.prefix_index]
            if not self.suffix_cursor.find_inside_of_prefix_replacement(existing_node):
                return self._add_new_nodes

            self.prefix_index -= 1
            if self.prefix_index == self.first_fork_index:
                return self._find_next_suffix_node_within_prefix_at_fork

    def _find_next_suffix_nodes(self):
        """Find all remaining suffix nodes in the chain.

        In this state, there's no (longer) any fork, so there's no other words
        using our current prefix. Therefore, as we find replacement nodes as we
        work our way backwards, we can remove the now-unused prefix nodes.
        """
        while True:
            existing_node = self.stack[self.prefix_index]
            if not self.suffix_cursor.find_end_of_prefix_replacement(existing_node):
                return self._add_new_nodes

            # This prefix node is wholly replaced by the new suffix node, so it can
            # be deleted.
            existing_node.remove()
            self.prefix_index -= 1

    def _add_new_nodes(self):
        """Adds new nodes to support the new word.

        Duplicates forked nodes to make room for new links, adds new nodes for new
        characters, and splices the prefix to the suffix to finish embedding the new
        word into the DAFSA.
        """
        if self.first_fork_index is not None:
            front_node = _duplicate_fork_nodes(
                self.stack,
                self.first_fork_index,
                self.prefix_index,
                # if suffix_overlaps_parent, the parent link was removed
                # earlier in the word-adding process.
                remove_parent_link=not self.suffix_overlaps_prefix,
            )
        else:
            front_node = self.stack[self.prefix_index]

        new_text = self.word[self.prefix_index : self.suffix_cursor.index - 1]
        for character in new_text:
            new_node = Node(character)
            front_node.add_child(new_node)
            front_node = new_node

        front_node.add_child(self.suffix_cursor.node)
        return None  # Done!


def _duplicate_fork_nodes(stack, fork_index, prefix_index, remove_parent_link=True):
    parent_node = stack[fork_index - 1]
    if remove_parent_link:
        # remove link to old chain that we're going to be copying
        stack[fork_index].remove_parent(parent_node)

    for index in range(fork_index, prefix_index + 1):
        fork_node = stack[index]
        replacement_node = Node(fork_node.character)
        child_to_avoid = None
        if index < len(stack) - 1:
            # We're going to be manually replacing the next node in the stack,
            # so don't connect it as a child.
            child_to_avoid = stack[index + 1]

        replacement_node.copy_fork_node(fork_node, child_to_avoid)
        parent_node.add_child(replacement_node)
        parent_node = replacement_node

    return parent_node


class Dafsa:
    root_node: Node
    end_node: Node

    def __init__(self):
        self.root_node = Node(None, is_root_node=True)
        self.end_node = Node(None, is_end_node=True)

    @classmethod
    def from_tld_data(cls, lines):
        """Create a dafsa for TLD data.

        TLD data has a domain and a "type" enum. The source data encodes the type as a
        text number, but the dafsa-consuming code assumes that the type is a raw byte
        number (e.g.: "1" => 0x01).

        This function acts as a helper, processing this TLD detail before creating a
        standard dafsa.
        """

        dafsa = cls()
        for i, word in enumerate(lines):
            domain_number = word[-1]
            # Convert type from string to byte representation
            raw_domain_number = chr(ord(domain_number) & 0x0F)

            word = "%s%s" % (word[:-1], raw_domain_number)
            dafsa.append(word)
        return dafsa

    def append(self, word):
        state_machine = DafsaAppendStateMachine(word, self.root_node, self.end_node)
        state_machine.run()
