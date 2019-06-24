import sys

import pytest

from .. import expectedtree, metadata

def dump_tree(tree):
    rv = []

    def dump_node(node, indent=0):
        prefix = " " * indent
        if not node.prop:
            data = "root"
        else:
            data = "%s:%s" % (node.prop, node.value)
        if node.result_values:
            data += " result_values:%s" % (",".join(sorted(node.result_values)))
        rv.append("%s<%s>" % (prefix, data))
        for child in sorted(node.children, key=lambda x:x.value):
            dump_node(child, indent + 2)

    dump_node(tree)
    return "\n".join(rv)


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_build_tree_0():
    # Pass iff debug
    results = [({"os": "linux", "version": "18.04", "debug": True}, "FAIL"),
               ({"os": "linux", "version": "18.04", "debug": False}, "PASS"),
               ({"os": "linux", "version": "16.04", "debug": False}, "PASS"),
               ({"os": "mac", "version": "10.12", "debug": True}, "FAIL"),
               ({"os": "mac", "version": "10.12", "debug": False}, "PASS"),
               ({"os": "win", "version": "7", "debug": False}, "PASS"),
               ({"os": "win", "version": "10", "debug": False}, "PASS")]
    results = {metadata.RunInfo(run_info): set([status]) for run_info, status in results}
    tree = expectedtree.build_tree(["os", "version", "debug"], {}, results)

    expected = """<root>
  <debug:False result_values:PASS>
  <debug:True result_values:FAIL>"""

    assert dump_tree(tree) == expected


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_build_tree_1():
    # Pass if linux or windows 10
    results = [({"os": "linux", "version": "18.04", "debug": True}, "PASS"),
               ({"os": "linux", "version": "18.04", "debug": False}, "PASS"),
               ({"os": "linux", "version": "16.04", "debug": False}, "PASS"),
               ({"os": "mac", "version": "10.12", "debug": True}, "FAIL"),
               ({"os": "mac", "version": "10.12", "debug": False}, "FAIL"),
               ({"os": "win", "version": "7", "debug": False}, "FAIL"),
               ({"os": "win", "version": "10", "debug": False}, "PASS")]
    results = {metadata.RunInfo(run_info): set([status]) for run_info, status in results}
    tree = expectedtree.build_tree(["os", "debug"], {"os": ["version"]}, results)

    expected = """<root>
  <os:linux result_values:PASS>
  <os:mac result_values:FAIL>
  <os:win>
    <version:10 result_values:PASS>
    <version:7 result_values:FAIL>"""

    assert dump_tree(tree) == expected


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_build_tree_2():
    # Fails in a specific configuration
    results = [({"os": "linux", "version": "18.04", "debug": True}, "PASS"),
               ({"os": "linux", "version": "18.04", "debug": False}, "FAIL"),
               ({"os": "linux", "version": "16.04", "debug": False}, "PASS"),
               ({"os": "linux", "version": "16.04", "debug": True}, "PASS"),
               ({"os": "mac", "version": "10.12", "debug": True}, "PASS"),
               ({"os": "mac", "version": "10.12", "debug": False}, "PASS"),
               ({"os": "win", "version": "7", "debug": False}, "PASS"),
               ({"os": "win", "version": "10", "debug": False}, "PASS")]
    results = {metadata.RunInfo(run_info): set([status]) for run_info, status in results}
    tree = expectedtree.build_tree(["os", "debug"], {"os": ["version"]}, results)

    expected = """<root>
  <os:linux>
    <debug:False>
      <version:16.04 result_values:PASS>
      <version:18.04 result_values:FAIL>
    <debug:True result_values:PASS>
  <os:mac result_values:PASS>
  <os:win result_values:PASS>"""

    assert dump_tree(tree) == expected


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_build_tree_3():

    results = [({"os": "linux", "version": "18.04", "debug": True, "unused": False}, "PASS"),
               ({"os": "linux", "version": "18.04", "debug": True, "unused": True}, "FAIL")]
    results = {metadata.RunInfo(run_info): set([status]) for run_info, status in results}
    tree = expectedtree.build_tree(["os", "debug"], {"os": ["version"]}, results)

    expected = """<root result_values:FAIL,PASS>"""

    assert dump_tree(tree) == expected
