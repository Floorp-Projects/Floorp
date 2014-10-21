# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import sys
import tempfile
import types
import uuid
from collections import defaultdict

from mozlog.structured import reader
from mozlog.structured import structuredlog

import expected
import manifestupdate
import wptmanifest
import wpttest
from vcs import git
manifest = None  # Module that will be imported relative to test_root

logger = structuredlog.StructuredLogger("web-platform-tests")


def manifest_path(metadata_root):
    return os.path.join(metadata_root, "MANIFEST.json")


def load_test_manifest(test_root, metadata_root):
    do_test_relative_imports(test_root)
    return manifest.load(manifest_path(metadata_root))


def update_manifest(git_root, metadata_root):
    manifest.setup_git(git_root)
    # Create an entirely new manifest
    new_manifest = manifest.Manifest(None)
    manifest.update(new_manifest)
    manifest.write(new_manifest, manifest_path(metadata_root))
    return new_manifest


def update_expected(test_root, metadata_root, log_file_names, rev_old=None, rev_new="HEAD",
                    ignore_existing=False, sync_root=None):
    """Update the metadata files for web-platform-tests based on
    the results obtained in a previous run"""

    manifest = load_test_manifest(test_root, metadata_root)

    if sync_root is not None:
        if rev_old is not None:
            rev_old = git("rev-parse", rev_old, repo=test_root).strip()
        rev_new = git("rev-parse", rev_new, repo=test_root).strip()

    if rev_old is not None:
        change_data = load_change_data(rev_old, rev_new, repo=test_root)
    else:
        change_data = {}

    expected_map = update_from_logs(metadata_root, manifest, *log_file_names,
                                    ignore_existing=ignore_existing)

    write_changes(metadata_root, expected_map)

    results_changed = [item.test_path for item in expected_map.itervalues() if item.modified]

    return unexpected_changes(change_data, results_changed)


def do_test_relative_imports(test_root):
    global manifest

    sys.path.insert(0, os.path.join(test_root))
    sys.path.insert(0, os.path.join(test_root, "tools", "scripts"))
    import manifest


def files_in_repo(repo_root):
    return git("ls-tree", "-r", "--name-only", "HEAD").split("\n")


def rev_range(rev_old, rev_new, symmetric=False):
    joiner = ".." if not symmetric else "..."
    return "".join([rev_old, joiner, rev_new])


def paths_changed(rev_old, rev_new, repo):
    data = git("diff", "--name-status", rev_range(rev_old, rev_new), repo=repo)
    lines = [tuple(item.strip() for item in line.strip().split("\t", 1))
             for line in data.split("\n") if line.strip()]
    output = set(lines)
    return output


def load_change_data(rev_old, rev_new, repo):
    changes = paths_changed(rev_old, rev_new, repo)
    rv = {}
    status_keys = {"M": "modified",
                   "A": "new",
                   "D": "deleted"}
    # TODO: deal with renames
    for item in changes:
        rv[item[1]] = status_keys[item[0]]
    return rv


def unexpected_changes(change_data, files_changed):
    return [fn for fn in files_changed if change_data.get(fn) != "M"]

# For each testrun
# Load all files and scan for the suite_start entry
# Build a hash of filename: properties
# For each different set of properties, gather all chunks
# For each chunk in the set of chunks, go through all tests
# for each test, make a map of {conditionals: [(platform, new_value)]}
# Repeat for each platform
# For each test in the list of tests:
#   for each conditional:
#      If all the new values match (or there aren't any) retain that conditional
#      If any new values mismatch mark the test as needing human attention
#   Check if all the RHS values are the same; if so collapse the conditionals


def update_from_logs(metadata_path, manifest, *log_filenames, **kwargs):
    ignore_existing = kwargs.pop("ignore_existing", False)

    expected_map, id_path_map = create_test_tree(metadata_path, manifest)
    updater = ExpectedUpdater(expected_map, id_path_map, ignore_existing=ignore_existing)
    for log_filename in log_filenames:
        with open(log_filename) as f:
            updater.update_from_log(f)

    for tree in expected_map.itervalues():
        for test in tree.iterchildren():
            for subtest in test.iterchildren():
                subtest.coalesce_expected()
            test.coalesce_expected()

    return expected_map


def write_changes(metadata_path, expected_map):
    # First write the new manifest files to a temporary directory
    temp_path = tempfile.mkdtemp()
    write_new_expected(temp_path, expected_map)
    shutil.copyfile(os.path.join(metadata_path, "MANIFEST.json"),
                    os.path.join(temp_path, "MANIFEST.json"))

    # Then move the old manifest files to a new location
    temp_path_2 = metadata_path + str(uuid.uuid4())
    os.rename(metadata_path, temp_path_2)
    # Move the new files to the destination location and remove the old files
    os.rename(temp_path, metadata_path)
    shutil.rmtree(temp_path_2)


def write_new_expected(metadata_path, expected_map):
    # Serialize the data back to a file
    for tree in expected_map.itervalues():
        if not tree.is_empty:
            manifest_str = wptmanifest.serialize(tree.node, skip_empty_data=True)
            assert manifest_str != ""
            path = expected.expected_path(metadata_path, tree.test_path)
            dir = os.path.split(path)[0]
            if not os.path.exists(dir):
                os.makedirs(dir)
            with open(path, "w") as f:
                f.write(manifest_str.encode("utf8"))


class ExpectedUpdater(object):
    def __init__(self, expected_tree, id_path_map, ignore_existing=False):
        self.expected_tree = expected_tree
        self.id_path_map = id_path_map
        self.ignore_existing = ignore_existing
        self.run_info = None
        self.action_map = {"suite_start": self.suite_start,
                           "test_start": self.test_start,
                           "test_status": self.test_status,
                           "test_end": self.test_end}
        self.tests_visited = {}

        self.test_cache = {}

    def update_from_log(self, log_file):
        self.run_info = None
        log_reader = reader.read(log_file)
        reader.each_log(log_reader, self.action_map)

    def suite_start(self, data):
        self.run_info = data["run_info"]

    def test_id(self, id):
        if type(id) in types.StringTypes:
            return id
        else:
            return tuple(id)

    def test_start(self, data):
        test_id = self.test_id(data["test"])
        try:
            test = self.expected_tree[self.id_path_map[test_id]].get_test(test_id)
        except KeyError:
            print "Test not found %s, skipping" % test_id
            return
        self.test_cache[test_id] = test

        if test_id not in self.tests_visited:
            if self.ignore_existing:
                test.clear_expected()
            self.tests_visited[test_id] = set()

    def test_status(self, data):
        test_id = self.test_id(data["test"])
        test = self.test_cache.get(test_id)
        if test is None:
            return
        test_cls = wpttest.manifest_test_cls[test.test_type]

        subtest = test.get_subtest(data["subtest"])

        self.tests_visited[test.id].add(data["subtest"])

        result = test_cls.subtest_result_cls(
            data["subtest"],
            data["status"],
            data.get("message"))

        subtest.set_result(self.run_info, result)

    def test_end(self, data):
        test_id = self.test_id(data["test"])
        test = self.test_cache.get(test_id)
        if test is None:
            return
        test_cls = wpttest.manifest_test_cls[test.test_type]

        result = test_cls.result_cls(
            data["status"],
            data.get("message"))

        test.set_result(self.run_info, result)
        del self.test_cache[test_id]


def create_test_tree(metadata_path, manifest):
    expected_map = {}
    test_id_path_map = {}
    exclude_types = frozenset(["stub", "helper", "manual"])
    include_types = set(manifest.item_types) ^ exclude_types
    for test_path, tests in manifest.itertypes(*include_types):

        expected_data = load_expected(metadata_path, test_path, tests)
        if expected_data is None:
            expected_data = create_expected(test_path, tests)

        expected_map[test_path] = expected_data

        for test in tests:
            test_id_path_map[test.id] = test_path

    return expected_map, test_id_path_map


def create_expected(test_path, tests):
    expected = manifestupdate.ExpectedManifest(None, test_path)
    for test in tests:
        expected.append(manifestupdate.TestNode.create(test.item_type, test.id))
    return expected


def load_expected(metadata_path, test_path, tests):
    expected_manifest = manifestupdate.get_manifest(metadata_path, test_path)
    if expected_manifest is None:
        return

    tests_by_id = {item.id: item for item in tests}

    # Remove expected data for tests that no longer exist
    for test in expected_manifest.iterchildren():
        if not test.id in tests_by_id:
            test.remove()

    # Add tests that don't have expected data
    for test in tests:
        if not expected_manifest.has_test(test.id):
            expected_manifest.append(manifestupdate.TestNode.create(test.item_type, test.id))

    return expected_manifest
