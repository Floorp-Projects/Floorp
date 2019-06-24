from __future__ import print_function
import itertools
import os
from six.moves.urllib.parse import urljoin
from collections import namedtuple, defaultdict, deque
from math import ceil

from wptmanifest import serialize
from wptmanifest.node import (DataNode, ConditionalNode, BinaryExpressionNode,
                              BinaryOperatorNode, NumberNode, StringNode, VariableNode,
                              ValueNode, UnaryExpressionNode, UnaryOperatorNode, KeyValueNode,
                              ListNode)
from wptmanifest.backends import conditional
from wptmanifest.backends.conditional import ManifestItem

import expected
import expectedtree

"""Manifest structure used to update the expected results of a test

Each manifest file is represented by an ExpectedManifest that has one
or more TestNode children, one per test in the manifest.  Each
TestNode has zero or more SubtestNode children, one for each known
subtest of the test.

In these representations, conditionals expressions in the manifest are
not evaluated upfront but stored as python functions to be evaluated
at runtime.

When a result for a test is to be updated set_result on the
[Sub]TestNode is called to store the new result, alongside the
existing conditional that result's run info matched, if any. Once all
new results are known, coalesce_expected is called to compute the new
set of results and conditionals. The AST of the underlying parsed manifest
is updated with the changes, and the result is serialised to a file.
"""


class ConditionError(Exception):
    def __init__(self, cond=None):
        self.cond = cond


class UpdateError(Exception):
    pass


Value = namedtuple("Value", ["run_info", "value"])


def data_cls_getter(output_node, visited_node):
    # visited_node is intentionally unused
    if output_node is None:
        return ExpectedManifest
    elif isinstance(output_node, ExpectedManifest):
        return TestNode
    elif isinstance(output_node, TestNode):
        return SubtestNode
    else:
        raise ValueError


class UpdateProperties(object):
    def __init__(self, manifest, **kwargs):
        self._manifest = manifest
        self._classes = kwargs

    def __getattr__(self, name):
        if name in self._classes:
            rv = self._classes[name](self._manifest)
            setattr(self, name, rv)
            return rv
        raise AttributeError

    def __contains__(self, name):
        return name in self._classes

    def __iter__(self):
        for name in self._classes.iterkeys():
            yield getattr(self, name)


class ExpectedManifest(ManifestItem):
    def __init__(self, node, test_path, url_base, run_info_properties):
        """Object representing all the tests in a particular manifest

        :param node: AST Node associated with this object. If this is None,
                     a new AST is created to associate with this manifest.
        :param test_path: Path of the test file associated with this manifest.
        :param url_base: Base url for serving the tests in this manifest.
        :param run_info_properties: Tuple of ([property name],
                                              {property_name: [dependent property]})
                                    The first part lists run_info properties
                                    that are always used in the update, the second
                                    maps property names to additional properties that
                                    can be considered if we already have a condition on
                                    the key property e.g. {"foo": ["bar"]} means that
                                    we consider making conditions on bar only after we
                                    already made one on foo.
        """
        if node is None:
            node = DataNode(None)
        ManifestItem.__init__(self, node)
        self.child_map = {}
        self.test_path = test_path
        self.url_base = url_base
        assert self.url_base is not None
        self._modified = False
        self.run_info_properties = run_info_properties

        self.update_properties = UpdateProperties(self, **{
            "lsan": LsanUpdate,
            "leak_object": LeakObjectUpdate,
            "leak_threshold": LeakThresholdUpdate,
        })

    @property
    def modified(self):
        if self._modified:
            return True
        return any(item.modified for item in self.children)

    @modified.setter
    def modified(self, value):
        self._modified = value

    def append(self, child):
        ManifestItem.append(self, child)
        if child.id in self.child_map:
            print("Warning: Duplicate heading %s" % child.id)
        self.child_map[child.id] = child

    def _remove_child(self, child):
        del self.child_map[child.id]
        ManifestItem._remove_child(self, child)

    def get_test(self, test_id):
        """Return a TestNode by test id, or None if no test matches

        :param test_id: The id of the test to look up"""

        return self.child_map.get(test_id)

    def has_test(self, test_id):
        """Boolean indicating whether the current test has a known child test
        with id test id

        :param test_id: The id of the test to look up"""

        return test_id in self.child_map

    @property
    def url(self):
        return urljoin(self.url_base,
                       "/".join(self.test_path.split(os.path.sep)))

    def set_lsan(self, run_info, result):
        """Set the result of the test in a particular run

        :param run_info: Dictionary of run_info parameters corresponding
                         to this run
        :param result: Lsan violations detected"""
        self.update_properties.lsan.set(run_info, result)

    def set_leak_object(self, run_info, result):
        """Set the result of the test in a particular run

        :param run_info: Dictionary of run_info parameters corresponding
                         to this run
        :param result: Leaked objects deletec"""
        self.update_properties.leak_object.set(run_info, result)

    def set_leak_threshold(self, run_info, result):
        """Set the result of the test in a particular run

        :param run_info: Dictionary of run_info parameters corresponding
                         to this run
        :param result: Total number of bytes leaked"""
        self.update_properties.leak_threshold.set(run_info, result)

    def update(self, stability):
        for prop_update in self.update_properties:
            prop_update.update(stability)


class TestNode(ManifestItem):
    def __init__(self, node):
        """Tree node associated with a particular test in a manifest

        :param node: AST node associated with the test"""

        ManifestItem.__init__(self, node)
        self.subtests = {}
        self._from_file = True
        self.new_disabled = False
        self.has_result = False
        self.modified = False
        self.update_properties = UpdateProperties(
            self,
            expected=ExpectedUpdate,
            max_asserts=MaxAssertsUpdate,
            min_asserts=MinAssertsUpdate
        )

    @classmethod
    def create(cls, test_id):
        """Create a TestNode corresponding to a given test

        :param test_type: The type of the test
        :param test_id: The id of the test"""

        url = test_id
        name = url.rsplit("/", 1)[1]
        node = DataNode(name)
        self = cls(node)

        self._from_file = False
        return self

    @property
    def is_empty(self):
        ignore_keys = {"type"}
        if set(self._data.keys()) - ignore_keys:
            return False
        return all(child.is_empty for child in self.children)

    @property
    def test_type(self):
        """The type of the test represented by this TestNode"""
        return self.get("type", None)

    @property
    def id(self):
        """The id of the test represented by this TestNode"""
        return urljoin(self.parent.url, self.name)

    def disabled(self, run_info):
        """Boolean indicating whether this test is disabled when run in an
        environment with the given run_info

        :param run_info: Dictionary of run_info parameters"""

        return self.get("disabled", run_info) is not None

    def set_result(self, run_info, result):
        """Set the result of the test in a particular run

        :param run_info: Dictionary of run_info parameters corresponding
                         to this run
        :param result: Status of the test in this run"""

        self.update_properties.expected.set(run_info, result)

    def set_asserts(self, run_info, count):
        """Set the assert count of a test

        """
        self.update_properties.min_asserts.set(run_info, count)
        self.update_properties.max_asserts.set(run_info, count)

    def append(self, node):
        child = ManifestItem.append(self, node)
        self.subtests[child.name] = child

    def get_subtest(self, name):
        """Return a SubtestNode corresponding to a particular subtest of
        the current test, creating a new one if no subtest with that name
        already exists.

        :param name: Name of the subtest"""

        if name in self.subtests:
            return self.subtests[name]
        else:
            subtest = SubtestNode.create(name)
            self.append(subtest)
            return subtest

    def update(self, stability):
        for prop_update in self.update_properties:
            prop_update.update(stability)


class SubtestNode(TestNode):
    def __init__(self, node):
        assert isinstance(node, DataNode)
        TestNode.__init__(self, node)

    @classmethod
    def create(cls, name):
        node = DataNode(name)
        self = cls(node)
        return self

    @property
    def is_empty(self):
        if self._data:
            return False
        return True


def build_conditional_tree(_, run_info_properties, results):
    properties, dependent_props = run_info_properties
    return expectedtree.build_tree(properties, dependent_props, results)


def build_unconditional_tree(_, run_info_properties, results):
    root = expectedtree.Node(None, None)
    for run_info, value in results.iteritems():
        root.result_values |= value
        root.run_info.add(run_info)
    return root


class PropertyUpdate(object):
    property_name = None
    cls_default_value = None
    value_type = None
    property_builder = None

    def __init__(self, node):
        self.node = node
        self.default_value = self.cls_default_value
        self.has_result = False
        self.results = defaultdict(set)

    def run_info_by_condition(self, run_info_index, conditions):
        run_info_by_condition = defaultdict(list)
        # A condition might match 0 or more run_info values
        run_infos = run_info_index.keys()
        for cond in conditions:
            for run_info in run_infos:
                if cond(run_info):
                    run_info_by_condition[cond].append(run_info)

        return run_info_by_condition

    def set(self, run_info, value):
        self.has_result = True
        self.node.has_result = True
        self.check_default(value)
        value = self.from_result_value(value)
        self.results[run_info].add(value)

    def check_default(self, result):
        return

    def from_result_value(self, value):
        """Convert a value from a test result into the internal format"""
        return value

    def from_ini_value(self, value):
        """Convert a value from an ini file into the internal format"""
        if self.value_type:
            return self.value_type(value)
        return value

    def to_ini_value(self, value):
        """Convert a value from the internal format to the ini file format"""
        return str(value)

    def updated_value(self, current, new):
        """Given a single current value and a set of observed new values,
        compute an updated value for the property"""
        return new

    @property
    def unconditional_value(self):
        try:
            unconditional_value = self.from_ini_value(
                self.node.get(self.property_name))
        except KeyError:
            unconditional_value = self.default_value
        return unconditional_value

    def update(self, stability=None, full_update=False):
        """Update the underlying manifest AST for this test based on all the
        added results.

        This will update existing conditionals if they got the same result in
        all matching runs in the updated results, will delete existing conditionals
        that get more than one different result in the updated run, and add new
        conditionals for anything that doesn't match an existing conditional.

        Conditionals not matched by any added result are not changed.

        When `stability` is not None, disable any test that shows multiple
        unexpected results for the same set of parameters.
        """
        if not self.has_result:
            return

        property_tree = self.property_builder(self.node.root.run_info_properties,
                                              self.results)

        conditions, errors = self.update_conditions(property_tree, full_update)

        for e in errors:
            if stability and e.cond:
                self.node.set("disabled", stability or "unstable",
                              e.cond.children[0])
                self.node.new_disabled = True
            else:
                msg = "Conflicting metadata values for %s" % (
                    self.node.root.test_path)
                if e.cond:
                    msg += ": %s" % serialize(e.cond).strip()
                print(msg)

        if self.node.modified:
            self.node.clear(self.property_name)

            for condition, value in conditions:
                if condition is None or value != self.unconditional_value:
                    self.node.set(self.property_name,
                                  self.to_ini_value(value),
                                  condition
                                  if condition is not None else None)

    def update_conditions(self, property_tree, full_update):
        current_conditions = self.node.get_conditions(self.property_name)
        conditions = []
        errors = []

        run_info_index = {run_info: node
                          for node in property_tree
                          for run_info in node.run_info}

        node_by_run_info = {run_info: node
                            for (run_info, node) in run_info_index.iteritems()
                            if node.result_values}

        run_info_by_condition = self.run_info_by_condition(run_info_index,
                                                           current_conditions)

        run_info_with_condition = set()

        # Retain existing conditions if they match the updated values
        for condition in current_conditions:
            # All run_info that isn't handled by some previous condition
            all_run_infos_condition = run_info_by_condition[condition]
            run_infos = {item for item in all_run_infos_condition
                         if item not in run_info_with_condition}

            if not run_infos:
                # Retain existing conditions that don't match anything in the update
                if not full_update:
                    conditions.append((condition.condition_node,
                                       self.from_ini_value(condition.value)))
                continue

            # Set of nodes in the updated tree that match the same run_info values as the
            # current existing node
            nodes = [node_by_run_info[run_info] for run_info in run_infos
                     if run_info in node_by_run_info]
            # If all the values are the same, update the value
            if nodes and all(node.result_values == nodes[0].result_values for node in nodes):
                current_value = self.from_ini_value(condition.value)
                try:
                    new_value = self.updated_value(current_value,
                                                   nodes[0].result_values)
                except ConditionError as e:
                    errors.append(e)
                    continue
                if new_value != current_value:
                    self.node.modified = True
                conditions.append((condition.condition_node, new_value))
                run_info_with_condition |= set(run_infos)
            else:
                # Don't append this condition
                self.node.modified = True

        new_conditions, new_errors = self.build_tree_conditions(property_tree,
                                                                run_info_with_condition)
        conditions.extend(new_conditions)
        errors.extend(new_errors)

        # Re-add the default if there isn't one
        if (current_conditions and
            current_conditions[-1].condition_node is None and
            conditions[-1][0] is not None):
            conditions.append((current_conditions[-1].condition_node,
                               self.from_ini_value(current_conditions[-1].value)))

        # Don't add a condition to set the default whatever the class default is
        if (conditions and
            conditions[-1][0] is None and
            conditions[-1][1] == self.default_value):
            conditions = conditions[:-1]

        if full_update:
            # Check if any new conditions match what's going to become the
            # unconditional value
            new_unconditional_value = (conditions[-1][1]
                                       if conditions[-1][0] is None
                                       else self.unconditional_value)

            if any(item[1] == new_unconditional_value for item in conditions
                   if item[0] is not None):
                self.remove_default(run_info_index, conditions, new_unconditional_value)
        return conditions, errors

    def build_tree_conditions(self, property_tree, run_info_with_condition):
        conditions = []
        errors = []

        queue = deque([(property_tree, [])])
        while queue:
            node, parents = queue.popleft()
            parents_and_self = parents + [node]
            if node.result_values and any(run_info not in run_info_with_condition
                                          for run_info in node.run_info):
                prop_set = [(item.prop, item.value) for item in parents_and_self if item.prop]
                value = node.result_values
                error = None
                if parents:
                    try:
                        value = self.updated_value(None, value)
                    except ConditionError:
                        expr = make_expr(prop_set, value)
                        error = ConditionError(expr)
                    expr = make_expr(prop_set, value)
                else:
                    # The root node needs special handling
                    expr = None
                    value = self.updated_value(self.unconditional_value,
                                               value)
                if error is None:
                    conditions.append((expr, value))
                else:
                    errors.append(error)

            for child in node.children:
                queue.append((child, parents_and_self))

        if conditions:
            self.node.modified = True
        return conditions[::-1], errors

    def remove_default(self, run_info_index, conditions, new_unconditional_value):
        # Remove any conditions that match the default value and won't
        # be overridden by a later conditional
        conditions = []
        matched_run_info = set()
        run_infos = self.run_info_index.keys()
        for idx, (cond, value) in enumerate(reversed(conditions)):
            if not cond:
                continue
            run_info_for_cond = set(run_info for run_info in run_infos if cond(run_info))
            if (value == new_unconditional_value and
                not run_info_for_cond & matched_run_info):
                pass
            else:
                conditions.append((cond, value))
            matched_run_info |= run_info_for_cond
        return conditions[::-1]


class ExpectedUpdate(PropertyUpdate):
    property_name = "expected"
    property_builder = build_conditional_tree

    def check_default(self, result):
        if self.default_value is not None:
            assert self.default_value == result.default_expected
        else:
            self.default_value = result.default_expected

    def from_result_value(self, result):
        return result.status

    def updated_value(self, current, new):
        if len(new) > 1:
            raise ConditionError
        return list(new)[0]


class MaxAssertsUpdate(PropertyUpdate):
    """For asserts we always update the default value and never add new conditionals.
    The value we set as the default is the maximum the current default or one more than the
    number of asserts we saw in any configuration."""

    property_name = "max-asserts"
    cls_default_value = 0
    value_type = int
    property_builder = build_unconditional_tree

    def updated_value(self, current, new):
        if any(item > current for item in new):
            return max(new) + 1
        return current


class MinAssertsUpdate(PropertyUpdate):
    property_name = "min-asserts"
    cls_default_value = 0
    value_type = int
    property_builder = build_unconditional_tree

    def updated_value(self, current, new):
        if any(item < current for item in new):
            rv = min(new) - 1
        else:
            rv = current
        return max(rv, 0)


class AppendOnlyListUpdate(PropertyUpdate):
    cls_default_value = []
    property_builder = build_unconditional_tree

    def updated_value(self, current, new):
        if current is None:
            rv = set()
        else:
            rv = set(current)

        for item in new:
            if item is None:
                continue
            elif isinstance(item, (str, unicode)):
                rv.add(item)
            else:
                rv |= item

        return sorted(rv)


class LsanUpdate(AppendOnlyListUpdate):
    property_name = "lsan-allowed"
    property_builder = build_unconditional_tree

    def from_result_value(self, result):
        # If we have an allowed_match that matched, return None
        # This value is ignored later (because it matches the default)
        # We do that because then if we allow a failure in foo/__dir__.ini
        # we don't want to update foo/bar/__dir__.ini with the same rule
        if result[1]:
            return None
        # Otherwise return the topmost stack frame
        # TODO: there is probably some improvement to be made by looking for a "better" stack frame
        return result[0][0]

    def to_ini_value(self, value):
        return value


class LeakObjectUpdate(AppendOnlyListUpdate):
    property_name = "leak-allowed"
    property_builder = build_unconditional_tree

    def from_result_value(self, result):
        # If we have an allowed_match that matched, return None
        if result[1]:
            return None
        # Otherwise return the process/object name
        return result[0]


class LeakThresholdUpdate(PropertyUpdate):
    property_name = "leak-threshold"
    cls_default_value = {}
    property_builder = build_unconditional_tree

    def from_result_value(self, result):
        return result

    def to_ini_value(self, data):
        return ["%s:%s" % item for item in sorted(data.iteritems())]

    def from_ini_value(self, data):
        rv = {}
        for item in data:
            key, value = item.split(":", 1)
            rv[key] = int(float(value))
        return rv

    def updated_value(self, current, new):
        if current:
            rv = current.copy()
        else:
            rv = {}
        for process, leaked_bytes, threshold in new:
            # If the value is less than the threshold but there isn't
            # an old value we must have inherited the threshold from
            # a parent ini file so don't any anything to this one
            if process not in rv and leaked_bytes < threshold:
                continue
            if leaked_bytes > rv.get(process, 0):
                # Round up to nearest 50 kb
                boundary = 50 * 1024
                rv[process] = int(boundary * ceil(float(leaked_bytes) / boundary))
        return rv


def make_expr(prop_set, rhs):
    """Create an AST that returns the value ``status`` given all the
    properties in prop_set match.

    :param prop_set: tuple of (property name, value) pairs for each
                     property in this expression and the value it must match
    :param status: Status on RHS when all the given properties match
    """
    root = ConditionalNode()

    assert len(prop_set) > 0

    expressions = []
    for prop, value in prop_set:
        if value not in (True, False):
            expressions.append(
                BinaryExpressionNode(
                    BinaryOperatorNode("=="),
                    VariableNode(prop),
                    make_node(value))
                )
        else:
            if value:
                expressions.append(VariableNode(prop))
            else:
                expressions.append(
                    UnaryExpressionNode(
                        UnaryOperatorNode("not"),
                        VariableNode(prop)
                    ))
    if len(expressions) > 1:
        prev = expressions[-1]
        for curr in reversed(expressions[:-1]):
            node = BinaryExpressionNode(
                BinaryOperatorNode("and"),
                curr,
                prev)
            prev = node
    else:
        node = expressions[0]

    root.append(node)
    rhs_node = make_value_node(rhs)
    root.append(rhs_node)

    return root


def make_node(value):
    if type(value) in (int, float, long):
        node = NumberNode(value)
    elif type(value) in (str, unicode):
        node = StringNode(unicode(value))
    elif hasattr(value, "__iter__"):
        node = ListNode()
        for item in value:
            node.append(make_node(item))
    return node


def make_value_node(value):
    if type(value) in (int, float, long):
        node = ValueNode(value)
    elif type(value) in (str, unicode):
        node = ValueNode(unicode(value))
    elif hasattr(value, "__iter__"):
        node = ListNode()
        for item in value:
            node.append(make_node(item))
    return node


def get_manifest(metadata_root, test_path, url_base, run_info_properties):
    """Get the ExpectedManifest for a particular test path, or None if there is no
    metadata stored for that test path.

    :param metadata_root: Absolute path to the root of the metadata directory
    :param test_path: Path to the test(s) relative to the test root
    :param url_base: Base url for serving the tests in this manifest"""
    manifest_path = expected.expected_path(metadata_root, test_path)
    try:
        with open(manifest_path) as f:
            rv = compile(f, test_path, url_base,
                         run_info_properties)
    except IOError:
        return None
    return rv


def compile(manifest_file, test_path, url_base, run_info_properties):
    return conditional.compile(manifest_file,
                               data_cls_getter=data_cls_getter,
                               test_path=test_path,
                               url_base=url_base,
                               run_info_properties=run_info_properties)
