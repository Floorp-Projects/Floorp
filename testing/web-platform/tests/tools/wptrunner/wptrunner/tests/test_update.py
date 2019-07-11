import json
import mock
import os
import pytest
import sys
from io import BytesIO

from .. import metadata, manifestupdate
from ..update.update import WPTUpdate
from ..update.base import StepRunner, Step
from mozlog import structuredlog, handlers, formatters

here = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(here, os.pardir, os.pardir, os.pardir))
from manifest import manifest, item as manifest_item


def rel_path_to_test_url(rel_path):
    assert not os.path.isabs(rel_path)
    return rel_path.replace(os.sep, "/")


def SourceFileWithTest(path, hash, cls, *args):
    s = mock.Mock(rel_path=path, hash=hash)
    test = cls("/foobar", path, "/", rel_path_to_test_url(path), *args)
    s.manifest_items = mock.Mock(return_value=(cls.item_type, [test]))
    return s


item_classes = {"testharness": manifest_item.TestharnessTest,
                "reftest": manifest_item.RefTest,
                "reftest_node": manifest_item.RefTestNode,
                "manual": manifest_item.ManualTest,
                "stub": manifest_item.Stub,
                "wdspec": manifest_item.WebDriverSpecTest,
                "conformancechecker": manifest_item.ConformanceCheckerTest,
                "visual": manifest_item.VisualTest,
                "support": manifest_item.SupportFile}


default_run_info = {"debug": False, "os": "linux", "version": "18.04", "processor": "x86_64", "bits": 64}


def reset_globals():
    metadata.prop_intern.clear()
    metadata.run_info_intern.clear()
    metadata.status_intern.clear()


def get_run_info(overrides):
    run_info = default_run_info.copy()
    run_info.update(overrides)
    return run_info


def update(tests, *logs, **kwargs):
    full_update = kwargs.pop("full_update", False)
    stability = kwargs.pop("stability", False)
    assert not kwargs
    id_test_map, updater = create_updater(tests)

    for log in logs:
        log = create_log(log)
        updater.update_from_log(log)

    update_properties = (["debug", "os", "version", "processor"],
                         {"os": ["version"], "processor": "bits"})

    expected_data = {}
    metadata.load_expected = lambda _, __, test_path, *args: expected_data.get(test_path)
    for test_path, test_ids, test_type, manifest_str in tests:
        expected_data[test_path] = manifestupdate.compile(BytesIO(manifest_str),
                                                          test_path,
                                                          "/",
                                                          update_properties)

    return list(metadata.update_results(id_test_map,
                                        update_properties,
                                        stability,
                                        full_update))


def create_updater(tests, url_base="/", **kwargs):
    id_test_map = {}
    m = create_test_manifest(tests, url_base)

    reset_globals()
    id_test_map = metadata.create_test_tree(None, m)

    return id_test_map, metadata.ExpectedUpdater(id_test_map, **kwargs)


def create_log(entries):
    data = BytesIO()
    if isinstance(entries, list):
        logger = structuredlog.StructuredLogger("expected_test")
        handler = handlers.StreamHandler(data, formatters.JSONFormatter())
        logger.add_handler(handler)

        for item in entries:
            action, kwargs = item
            getattr(logger, action)(**kwargs)
        logger.remove_handler(handler)
    else:
        json.dump(entries, data)
    data.seek(0)
    return data


def suite_log(entries, run_info=None):
    _run_info = default_run_info.copy()
    if run_info:
        _run_info.update(run_info)
    return ([("suite_start", {"tests": [], "run_info": _run_info})] +
            entries +
            [("suite_end", {})])


def create_test_manifest(tests, url_base="/"):
    source_files = []
    for i, (test, _, test_type, _) in enumerate(tests):
        if test_type:
            source_files.append((SourceFileWithTest(test, str(i) * 40, item_classes[test_type]), True))
    m = manifest.Manifest()
    m.update(source_files)
    return m


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_0():
    tests = [("path/to/test.htm", ["/path/to/test.htm"], "testharness",
              """[test.htm]
  [test1]
    expected: FAIL""")]

    log = suite_log([("test_start", {"test": "/path/to/test.htm"}),
                     ("test_status", {"test": "/path/to/test.htm",
                                      "subtest": "test1",
                                      "status": "PASS",
                                      "expected": "FAIL"}),
                     ("test_end", {"test": "/path/to/test.htm",
                                   "status": "OK"})])

    updated = update(tests, log)

    assert len(updated) == 1
    assert updated[0][1].is_empty


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_1():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness",
              """[test.htm]
  [test1]
    expected: ERROR""")]

    log = suite_log([("test_start", {"test": test_id}),
                     ("test_status", {"test": test_id,
                                      "subtest": "test1",
                                      "status": "FAIL",
                                      "expected": "ERROR"}),
                     ("test_end", {"test": test_id,
                                   "status": "OK"})])

    updated = update(tests, log)

    new_manifest = updated[0][1]
    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get("expected", default_run_info) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_skip_0():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness",
              """[test.htm]
  [test1]
    expected: FAIL""")]

    log = suite_log([("test_start", {"test": test_id}),
                     ("test_status", {"test": test_id,
                                      "subtest": "test1",
                                      "status": "FAIL",
                                      "expected": "FAIL"}),
                     ("test_end", {"test": test_id,
                                   "status": "OK"})])

    updated = update(tests, log)
    assert not updated


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_new_subtest():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected: FAIL""")]

    log = suite_log([("test_start", {"test": test_id}),
                     ("test_status", {"test": test_id,
                                      "subtest": "test1",
                                      "status": "FAIL",
                                      "expected": "FAIL"}),
                     ("test_status", {"test": test_id,
                                      "subtest": "test2",
                                      "status": "FAIL",
                                      "expected": "PASS"}),
                     ("test_end", {"test": test_id,
                                   "status": "OK"})])
    updated = update(tests, log)
    new_manifest = updated[0][1]
    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get("expected", default_run_info) == "FAIL"
    assert new_manifest.get_test(test_id).children[1].get("expected", default_run_info) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_multiple_0():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected: FAIL""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "os": "osx"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "TIMEOUT",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "os": "linux"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    run_info_1 = default_run_info.copy()
    run_info_1.update({"debug": False, "os": "osx"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"debug": False, "os": "linux"})
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", {"debug": False, "os": "linux"}) == "TIMEOUT"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_multiple_1():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected: FAIL""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "osx"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "TIMEOUT",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    run_info_1 = default_run_info.copy()
    run_info_1.update({"os": "osx"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"os": "linux"})
    run_info_3 = default_run_info.copy()
    run_info_3.update({"os": "win"})

    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "TIMEOUT"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_3) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_multiple_2():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected: FAIL""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "os": "osx"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "TIMEOUT",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": True, "os": "osx"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()
    run_info_1.update({"debug": False, "os": "osx"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"debug": True, "os": "osx"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "TIMEOUT"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_multiple_3():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected:
      if debug: FAIL
      if not debug and os == "osx": TIMEOUT""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "os": "osx"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "TIMEOUT",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": True, "os": "osx"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()
    run_info_1.update({"debug": False, "os": "osx"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"debug": True, "os": "osx"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "TIMEOUT"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_ignore_existing():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected:
      if debug: TIMEOUT
      if not debug and os == "osx": NOTRUN""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "os": "linux"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": True, "os": "windows"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()
    run_info_1.update({"debug": False, "os": "linux"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"debug": False, "os": "osx"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "NOTRUN"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_new_test():
    test_id = "/path/to/test.html"
    tests = [("path/to/test.html", [test_id], "testharness", None)]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})])
    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()

    assert not new_manifest.is_empty
    assert new_manifest.get_test("test.html") is None
    assert len(new_manifest.get_test(test_id).children) == 1
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_duplicate():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """
[test.htm]
  expected: ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "PASS"})])
    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "FAIL"})])

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]
    run_info_1 = default_run_info.copy()

    assert new_manifest.get_test(test_id).get(
        "expected", run_info_1) == "ERROR"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_stability():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """
[test.htm]
  expected: ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "PASS"})])
    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "FAIL"})])

    updated = update(tests, log_0, log_1, stability="Some message")
    new_manifest = updated[0][1]
    run_info_1 = default_run_info.copy()

    assert new_manifest.get_test(test_id).get(
        "disabled", run_info_1) == "Some message"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_stability_conditional_instability():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """
[test.htm]
  expected: ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "PASS"})],
                      run_info={"os": "linux"})
    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "FAIL"})],
                      run_info={"os": "linux"})
    log_2 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "status": "FAIL"})],
                      run_info={"os": "mac"})

    updated = update(tests, log_0, log_1, log_2, stability="Some message")
    new_manifest = updated[0][1]
    run_info_1 = default_run_info.copy()
    run_info_1.update({"os": "linux"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"os": "mac"})

    assert new_manifest.get_test(test_id).get(
        "disabled", run_info_1) == "Some message"
    with pytest.raises(KeyError):
        assert new_manifest.get_test(test_id).get(
            "disabled", run_info_2)
    assert new_manifest.get_test(test_id).get(
        "expected", run_info_2) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_full():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected:
      if debug: TIMEOUT
      if not debug and os == "osx": NOTRUN

  [test2]
    expected: FAIL

[test.js]
  [test1]
    expected: FAIL
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "ERROR",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": True})

    updated = update(tests, log_0, log_1, full_update=True)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()
    run_info_1.update({"debug": False, "os": "win"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"debug": True, "os": "osx"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test("test.js") is None
    assert len(new_manifest.get_test(test_id).children) == 1
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "ERROR"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_full_unknown():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected:
      if release_or_beta: ERROR
      if not debug and os == "osx": NOTRUN
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": False, "release_or_beta": False})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "FAIL",
                                        "expected": "PASS"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"debug": True, "release_or_beta": False})

    updated = update(tests, log_0, log_1, full_update=True)
    new_manifest = updated[0][1]

    run_info_1 = default_run_info.copy()
    run_info_1.update({"release_or_beta": False})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"release_or_beta": True})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_1) == "FAIL"
    assert new_manifest.get_test(test_id).children[0].get(
        "expected", run_info_2) == "ERROR"



@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_default():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  [test1]
    expected:
      if os == "mac": FAIL
      ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "PASS",
                                        "expected": "FAIL"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "mac"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("test_status", {"test": test_id,
                                        "subtest": "test1",
                                        "status": "PASS",
                                        "expected": "ERROR"}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert new_manifest.is_empty


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_default_1():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """
[test.htm]
  expected:
    if os == "mac": TIMEOUT
    ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "expected": "ERROR",
                                     "status": "FAIL"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty

    run_info_1 = default_run_info.copy()
    run_info_1.update({"os": "mac"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"os": "win"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get(
        "expected", run_info_1) == "TIMEOUT"
    assert new_manifest.get_test(test_id).get(
        "expected", run_info_2) == "FAIL"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_default_2():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """
[test.htm]
  expected:
    if os == "mac": TIMEOUT
    ERROR""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("test_end", {"test": test_id,
                                     "expected": "ERROR",
                                     "status": "TIMEOUT"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty

    run_info_1 = default_run_info.copy()
    run_info_1.update({"os": "mac"})
    run_info_2 = default_run_info.copy()
    run_info_2.update({"os": "win"})

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get(
        "expected", run_info_1) == "TIMEOUT"
    assert new_manifest.get_test(test_id).get(
        "expected", run_info_2) == "TIMEOUT"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_assertion_count_0():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  max-asserts: 4
  min-asserts: 2
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 6,
                                            "min_expected": 2,
                                            "max_expected": 4}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})])

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get("max-asserts") == "7"
    assert new_manifest.get_test(test_id).get("min-asserts") == "2"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_assertion_count_1():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  max-asserts: 4
  min-asserts: 2
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 1,
                                            "min_expected": 2,
                                            "max_expected": 4}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})])

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get("max-asserts") == "4"
    assert new_manifest.get_test(test_id).has_key("min-asserts") is False


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_assertion_count_2():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  max-asserts: 4
  min-asserts: 2
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 3,
                                            "min_expected": 2,
                                            "max_expected": 4}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})])

    updated = update(tests, log_0)
    assert not updated


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_assertion_count_3():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]
  max-asserts: 4
  min-asserts: 2
""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 6,
                                            "min_expected": 2,
                                            "max_expected": 4}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "windows"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 7,
                                            "min_expected": 2,
                                            "max_expected": 4}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get("max-asserts") == "8"
    assert new_manifest.get_test(test_id).get("min-asserts") == "2"


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_assertion_count_4():
    test_id = "/path/to/test.htm"
    tests = [("path/to/test.htm", [test_id], "testharness", """[test.htm]""")]

    log_0 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 6,
                                            "min_expected": 0,
                                            "max_expected": 0}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "windows"})

    log_1 = suite_log([("test_start", {"test": test_id}),
                       ("assertion_count", {"test": test_id,
                                            "count": 7,
                                            "min_expected": 0,
                                            "max_expected": 0}),
                       ("test_end", {"test": test_id,
                                     "status": "OK"})],
                      run_info={"os": "linux"})

    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get_test(test_id).get("max-asserts") == "8"
    assert new_manifest.get_test(test_id).has_key("min-asserts") is False


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_lsan_0():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, "")]

    log_0 = suite_log([("lsan_leak", {"scope": "path/to/",
                                      "frames": ["foo", "bar"]})])


    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("lsan-allowed") == ["foo"]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_lsan_1():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, """
lsan-allowed: [foo]""")]

    log_0 = suite_log([("lsan_leak", {"scope": "path/to/",
                                      "frames": ["foo", "bar"]}),
                       ("lsan_leak", {"scope": "path/to/",
                                      "frames": ["baz", "foobar"]})])


    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("lsan-allowed") == ["baz", "foo"]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_lsan_2():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/__dir__", ["path/__dir__"], None, """
lsan-allowed: [foo]"""),
             ("path/to/__dir__", [dir_id], None, "")]

    log_0 = suite_log([("lsan_leak", {"scope": "path/to/",
                                      "frames": ["foo", "bar"],
                                      "allowed_match": ["foo"]}),
                       ("lsan_leak", {"scope": "path/to/",
                                      "frames": ["baz", "foobar"]})])


    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("lsan-allowed") == ["baz"]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_lsan_3():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, "")]

    log_0 = suite_log([("lsan_leak", {"scope": "path/to/",
                                      "frames": ["foo", "bar"]})],
                      run_info={"os": "win"})

    log_1 = suite_log([("lsan_leak", {"scope": "path/to/",
                                      "frames": ["baz", "foobar"]})],
                      run_info={"os": "linux"})


    updated = update(tests, log_0, log_1)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("lsan-allowed") == ["baz", "foo"]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_wptreport_0():
    tests = [("path/to/test.htm", ["/path/to/test.htm"], "testharness",
              """[test.htm]
  [test1]
    expected: FAIL""")]

    log = {"run_info": default_run_info.copy(),
           "results": [
               {"test": "/path/to/test.htm",
                "subtests": [{"name": "test1",
                              "status": "PASS",
                              "expected": "FAIL"}],
                "status": "OK"}
           ]}

    updated = update(tests, log)

    assert len(updated) == 1
    assert updated[0][1].is_empty


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_wptreport_1():
    tests = [("path/to/test.htm", ["/path/to/test.htm"], "testharness", ""),
             ("path/to/__dir__", ["path/to/__dir__"], None, "")]

    log = {"run_info": default_run_info.copy(),
           "results": [],
           "lsan_leaks": [{"scope": "path/to/",
                           "frames": ["baz", "foobar"]}]}

    updated = update(tests, log)

    assert len(updated) == 1
    assert updated[0][1].get("lsan-allowed") == ["baz"]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_leak_total_0():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, "")]

    log_0 = suite_log([("mozleak_total", {"scope": "path/to/",
                                          "process": "default",
                                          "bytes": 100,
                                          "threshold": 0,
                                          "objects": []})])

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("leak-threshold") == ['default:51200']


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_leak_total_1():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, "")]

    log_0 = suite_log([("mozleak_total", {"scope": "path/to/",
                                          "process": "default",
                                          "bytes": 100,
                                          "threshold": 1000,
                                          "objects": []})])

    updated = update(tests, log_0)
    assert not updated


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_leak_total_2():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, """
leak-total: 110""")]

    log_0 = suite_log([("mozleak_total", {"scope": "path/to/",
                                          "process": "default",
                                          "bytes": 100,
                                          "threshold": 110,
                                          "objects": []})])

    updated = update(tests, log_0)
    assert not updated


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_leak_total_3():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, """
leak-total: 100""")]

    log_0 = suite_log([("mozleak_total", {"scope": "path/to/",
                                          "process": "default",
                                          "bytes": 1000,
                                          "threshold": 100,
                                          "objects": []})])

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.get("leak-threshold") == ['default:51200']


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="metadata doesn't support py3")
def test_update_leak_total_4():
    test_id = "/path/to/test.htm"
    dir_id = "path/to/__dir__"
    tests = [("path/to/test.htm", [test_id], "testharness", ""),
             ("path/to/__dir__", [dir_id], None, """
leak-total: 110""")]

    log_0 = suite_log([
        ("lsan_leak", {"scope": "path/to/",
                       "frames": ["foo", "bar"]}),
        ("mozleak_total", {"scope": "path/to/",
                           "process": "default",
                           "bytes": 100,
                           "threshold": 110,
                           "objects": []})])

    updated = update(tests, log_0)
    new_manifest = updated[0][1]

    assert not new_manifest.is_empty
    assert new_manifest.has_key("leak-threshold") is False


def dump_tree(tree):
    rv = []

    def dump_node(node, indent=0):
        prefix = " " * indent
        if not node.prop:
            data = "root"
        else:
            data = "%s:%s" % (node.prop, node.value)
        if node.update_values:
            data += " update_values:%s" % (",".join(sorted(node.update_values)))
        rv.append("%s<%s>" % (prefix, data))
        for child in sorted(node.children, key=lambda x:x.value):
            dump_node(child, indent + 2)

    dump_node(tree)
    return "\n".join(rv)


# @pytest.mark.xfail(sys.version[0] == "3",
#                    reason="metadata doesn't support py3")
# def test_property_tree():
#     run_info_values = [{"os": "linux", "version": "18.04", "debug": False},
#                        {"os": "linux", "version": "18.04", "debug": True},
#                        {"os": "linux", "version": "16.04", "debug": False},
#                        {"os": "mac", "version": "10.12", "debug": True},
#                        {"os": "mac", "version": "10.12", "debug": False},
#                        {"os": "win", "version": "7", "debug": False},
#                        {"os": "win", "version": "10", "debug": False}]
#     run_info_values = [metadata.RunInfo(item) for item in run_info_values]
#     tree = metadata.build_property_tree(["os", "version", "debug"],
#                                         run_info_values)

#     expected = """<root>
#   <os:linux>
#     <version:16.04>
#     <version:18.04>
#       <debug:False>
#       <debug:True>
#   <os:mac>
#     <debug:False>
#     <debug:True>
#   <os:win>
#     <version:10>
#     <version:7>"""

#     assert dump_tree(tree) == expected


# @pytest.mark.xfail(sys.version[0] == "3",
#                    reason="metadata doesn't support py3")
# def test_propogate_up():
#     update_values = [({"os": "linux", "version": "18.04", "debug": False}, "FAIL"),
#                      ({"os": "linux", "version": "18.04", "debug": True}, "FAIL"),
#                      ({"os": "linux", "version": "16.04", "debug": False}, "FAIL"),
#                      ({"os": "mac", "version": "10.12", "debug": True}, "PASS"),
#                      ({"os": "mac", "version": "10.12", "debug": False}, "PASS"),
#                      ({"os": "win", "version": "7", "debug": False}, "PASS"),
#                      ({"os": "win", "version": "10", "debug": False}, "FAIL")]
#     update_values = {metadata.RunInfo(item[0]): item[1] for item in update_values}
#     tree = metadata.build_property_tree(["os", "version", "debug"],
#                                         update_values.keys())
#     for node in tree:
#         for run_info in node.run_info:
#             node.update_values.add(update_values[run_info])

#     optimiser = manifestupdate.OptimiseConditionalTree()
#     optimiser.propogate_up(tree)

#     expected = """<root>
#   <os:linux update_values:FAIL>
#   <os:mac update_values:PASS>
#   <os:win>
#     <version:10 update_values:FAIL>
#     <version:7 update_values:PASS>"""

#     assert dump_tree(tree) == expected


# @pytest.mark.xfail(sys.version[0] == "3",
#                    reason="metadata doesn't support py3")
# def test_common_properties():
#     update_values = [({"os": "linux", "version": "18.04", "debug": False}, "PASS"),
#                      ({"os": "linux", "version": "18.04", "debug": True}, "FAIL"),
#                      ({"os": "linux", "version": "16.04", "debug": False}, "PASS"),
#                      ({"os": "mac", "version": "10.12", "debug": True}, "FAIL"),
#                      ({"os": "mac", "version": "10.12", "debug": False}, "PASS"),
#                      ({"os": "win", "version": "7", "debug": False}, "PASS"),
#                      ({"os": "win", "version": "10", "debug": False}, "PASS")]
#     update_values = {metadata.RunInfo(item[0]): item[1] for item in update_values}
#     tree = metadata.build_property_tree(["os", "version", "debug"],
#                                         update_values.keys())
#     for node in tree:
#         for run_info in node.run_info:
#             node.update_values.add(update_values[run_info])

#     optimiser = manifestupdate.OptimiseConditionalTree()
#     optimiser.propogate_up(tree)

#     expected = """<root>
#   <os:linux>
#     <version:16.04 update_values:PASS>
#     <version:18.04>
#       <debug:False update_values:PASS>
#       <debug:True update_values:FAIL>
#   <os:mac>
#     <debug:False update_values:PASS>
#     <debug:True update_values:FAIL>
#   <os:win update_values:PASS>"""

#     assert dump_tree(tree) == expected


#     optimiser.common_properties(tree)

#     expected = """<root>
#   <os:linux>
#     <debug:False update_values:PASS>
#     <debug:True update_values:FAIL>
#   <os:mac update_values:PASS>
#     <debug:False update_values:PASS>
#     <debug:True update_values:FAIL>
#   <os:win update_values: PASS>"""
#     assert dump_tree(tree) == expected


class TestStep(Step):
    def create(self, state):
        test_id = "/path/to/test.htm"
        tests = [("path/to/test.htm", [test_id], "testharness", "")]
        state.foo = create_test_manifest(tests)


class UpdateRunner(StepRunner):
    steps = [TestStep]


@pytest.mark.xfail(sys.version[0] == "3",
                   reason="update.state doesn't support py3")
def test_update_pickle():
    logger = structuredlog.StructuredLogger("expected_test")
    args = {
        "test_paths": {
            "/": {"tests_path": os.path.abspath(os.path.join(here,
                                                             os.pardir,
                                                             os.pardir,
                                                             os.pardir,
                                                             os.pardir))},
        },
        "abort": False,
        "continue": False,
        "sync": False,
    }
    args2 = args.copy()
    args2["abort"] = True
    wptupdate = WPTUpdate(logger, **args2)
    wptupdate = WPTUpdate(logger, runner_cls=UpdateRunner, **args)
    wptupdate.run()
