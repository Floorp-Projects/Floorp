# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

DEFAULT_TIMEOUT = 10  # seconds
LONG_TIMEOUT = 60  # seconds

import os

import mozinfo


class Result(object):
    def __init__(self, status, message, expected=None):
        if status not in self.statuses:
            raise ValueError("Unrecognised status %s" % status)
        self.status = status
        self.message = message
        self.expected = expected


class SubtestResult(object):
    def __init__(self, name, status, message, expected=None):
        self.name = name
        if status not in self.statuses:
            raise ValueError("Unrecognised status %s" % status)
        self.status = status
        self.message = message
        self.expected = expected


class TestharnessResult(Result):
    default_expected = "OK"
    statuses = set(["OK", "ERROR", "TIMEOUT", "EXTERNAL-TIMEOUT", "CRASH"])


class ReftestResult(Result):
    default_expected = "PASS"
    statuses = set(["PASS", "FAIL", "ERROR", "TIMEOUT", "EXTERNAL-TIMEOUT", "CRASH"])


class TestharnessSubtestResult(SubtestResult):
    default_expected = "PASS"
    statuses = set(["PASS", "FAIL", "TIMEOUT", "NOTRUN"])


def get_run_info(metadata_root, product, **kwargs):
    if product == "b2g":
        return B2GRunInfo(metadata_root, product, **kwargs)
    else:
        return RunInfo(metadata_root, product, **kwargs)


class RunInfo(dict):
    def __init__(self, metadata_root, product, debug):
        self._update_mozinfo(metadata_root)
        self.update(mozinfo.info)
        self["product"] = product
        if not "debug" in self:
            self["debug"] = debug

    def _update_mozinfo(self, metadata_root):
        """Add extra build information from a mozinfo.json file in a parent
        directory"""
        path = metadata_root
        dirs = set()
        while path != os.path.expanduser('~'):
            if path in dirs:
                break
            dirs.add(path)
            path = os.path.split(path)[0]

        mozinfo.find_and_update_from_json(*dirs)

class B2GRunInfo(RunInfo):
    def __init__(self, *args, **kwargs):
        RunInfo.__init__(self, *args, **kwargs)
        self["os"] = "b2g"


class Test(object):
    result_cls = None
    subtest_result_cls = None

    def __init__(self, url, expected_metadata, timeout=None, path=None):
        self.url = url
        self._expected_metadata = expected_metadata
        self.timeout = timeout
        self.path = path

    def __eq__(self, other):
        return self.id == other.id

    @property
    def id(self):
        return self.url

    @property
    def keys(self):
        return tuple()

    def _get_metadata(self, subtest):
        if self._expected_metadata is None:
            return None

        if subtest is not None:
            metadata = self._expected_metadata.get_subtest(subtest)
        else:
            metadata = self._expected_metadata
        return metadata

    def disabled(self, subtest=None):
        metadata = self._get_metadata(subtest)
        if metadata is None:
            return False

        return metadata.disabled()

    def expected(self, subtest=None):
        if subtest is None:
            default = self.result_cls.default_expected
        else:
            default = self.subtest_result_cls.default_expected

        metadata = self._get_metadata(subtest)
        if metadata is None:
            return default

        try:
            return metadata.get("expected")
        except KeyError:
            return default


class TestharnessTest(Test):
    result_cls = TestharnessResult
    subtest_result_cls = TestharnessSubtestResult

    @property
    def id(self):
        return self.url


class ManualTest(Test):
    @property
    def id(self):
        return self.url


class ReftestTest(Test):
    result_cls = ReftestResult

    def __init__(self, url, ref_url, ref_type, expected, timeout=None, path=None):
        self.url = url
        self.ref_url = ref_url
        if ref_type not in ("==", "!="):
            raise ValueError
        self.ref_type = ref_type
        self._expected_metadata = expected
        self.timeout = timeout
        self.path = path

    @property
    def id(self):
        return self.url, self.ref_type, self.ref_url

    @property
    def keys(self):
        return ("reftype", "refurl")

manifest_test_cls = {"reftest": ReftestTest,
                     "testharness": TestharnessTest,
                     "manual": ManualTest}


def from_manifest(manifest_test, expected_metadata):
    test_cls = manifest_test_cls[manifest_test.item_type]

    timeout = LONG_TIMEOUT if manifest_test.timeout == "long" else DEFAULT_TIMEOUT

    if test_cls == ReftestTest:
        return test_cls(manifest_test.url,
                        manifest_test.ref_url,
                        manifest_test.ref_type,
                        expected_metadata,
                        timeout=timeout,
                        path=manifest_test.path)
    else:
        return test_cls(manifest_test.url,
                        expected_metadata,
                        timeout=timeout,
                        path=manifest_test.path)
