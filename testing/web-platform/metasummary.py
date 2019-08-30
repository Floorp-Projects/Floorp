import argparse
import json
import logging
import os
import re
import urlparse
from collections import defaultdict

import manifestupdate

from wptrunner import expected
from wptrunner.wptmanifest.serializer import serialize
from wptrunner.wptmanifest.backends import base

here = os.path.dirname(__file__)
logger = logging.getLogger(__name__)
yaml = None


class Compiler(base.Compiler):
    def visit_KeyValueNode(self, node):
        key_name = node.data
        values = []
        for child in node.children:
            values.append(self.visit(child))

        self.output_node.set(key_name, values)

    def visit_ConditionalNode(self, node):
        assert len(node.children) == 2
        # For conditional nodes, just return the subtree
        return node.children[0], self.visit(node.children[1])

    def visit_UnaryExpressionNode(self, node):
        raise NotImplementedError

    def visit_BinaryExpressionNode(self, node):
        raise NotImplementedError

    def visit_UnaryOperatorNode(self, node):
        raise NotImplementedError

    def visit_BinaryOperatorNode(self, node):
        raise NotImplementedError


class ExpectedManifest(base.ManifestItem):
    def __init__(self, node, test_path, url_base):
        """Object representing all the tests in a particular manifest

        :param name: Name of the AST Node associated with this object.
                     Should always be None since this should always be associated with
                     the root node of the AST.
        :param test_path: Path of the test file associated with this manifest.
        :param url_base: Base url for serving the tests in this manifest
        """
        if test_path is None:
            raise ValueError("ExpectedManifest requires a test path")
        if url_base is None:
            raise ValueError("ExpectedManifest requires a base url")
        base.ManifestItem.__init__(self, node)
        self.child_map = {}
        self.test_path = test_path
        self.url_base = url_base

    def append(self, child):
        """Add a test to the manifest"""
        base.ManifestItem.append(self, child)
        self.child_map[child.id] = child

    @property
    def url(self):
        return urlparse.urljoin(self.url_base,
                                "/".join(self.test_path.split(os.path.sep)))


class DirectoryManifest(base.ManifestItem):
    pass


class TestManifestItem(base.ManifestItem):
    def __init__(self, node, **kwargs):
        """Tree node associated with a particular test in a manifest

        :param name: name of the test"""
        base.ManifestItem.__init__(self, node)
        self.subtests = {}

    @property
    def id(self):
        return urlparse.urljoin(self.parent.url, self.name)

    def append(self, node):
        """Add a subtest to the current test

        :param node: AST Node associated with the subtest"""
        child = base.ManifestItem.append(self, node)
        self.subtests[child.name] = child

    def get_subtest(self, name):
        """Get the SubtestNode corresponding to a particular subtest, by name

        :param name: Name of the node to return"""
        if name in self.subtests:
            return self.subtests[name]
        return None


class SubtestManifestItem(TestManifestItem):
    pass


def data_cls_getter(output_node, visited_node):
    # visited_node is intentionally unused
    if output_node is None:
        return ExpectedManifest
    if isinstance(output_node, ExpectedManifest):
        return TestManifestItem
    if isinstance(output_node, TestManifestItem):
        return SubtestManifestItem
    raise ValueError


def get_manifest(metadata_root, test_path, url_base):
    """Get the ExpectedManifest for a particular test path, or None if there is no
    metadata stored for that test path.

    :param metadata_root: Absolute path to the root of the metadata directory
    :param test_path: Path to the test(s) relative to the test root
    :param url_base: Base url for serving the tests in this manifest
    :param run_info: Dictionary of properties of the test run for which the expectation
                     values should be computed.
    """
    manifest_path = expected.expected_path(metadata_root, test_path)
    try:
        with open(manifest_path) as f:
            return compile(f,
                           data_cls_getter=data_cls_getter,
                           test_path=test_path,
                           url_base=url_base)
    except IOError:
        return None


def get_dir_manifest(path):
    """Get the ExpectedManifest for a particular test path, or None if there is no
    metadata stored for that test path.

    :param path: Full path to the ini file
    :param run_info: Dictionary of properties of the test run for which the expectation
                     values should be computed.
    """
    try:
        with open(path) as f:
            return compile(f, data_cls_getter=lambda x,y: DirectoryManifest)
    except IOError:
        return None


def compile(stream, data_cls_getter=None, **kwargs):
    return base.compile(Compiler,
                        stream,
                        data_cls_getter=data_cls_getter,
                        **kwargs)


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", help="Directory to store output files")
    parser.add_argument("--meta-dir", help="Directory containing wpt-metadata "
                        "checkout to update.")
    return parser


def run(src_root, obj_root, logger_=None, **kwargs):
    logger_obj = logger_ if logger_ is not None else logger

    manifests = manifestupdate.run(src_root, obj_root, logger_obj, **kwargs)

    rv = {}
    dirs_seen = set()

    for meta_root, test_path, test_metadata in iter_tests(manifests):
        for dir_path in get_dir_paths(meta_root, test_path):
            if dir_path not in dirs_seen:
                dirs_seen.add(dir_path)
                dir_manifest = get_dir_manifest(dir_path)
                rel_path = os.path.relpath(dir_path, meta_root)
                if dir_manifest:
                    add_manifest(rv, rel_path, dir_manifest)
            else:
                break
        add_manifest(rv, test_path, test_metadata)

    if kwargs["out_dir"]:
        if not os.path.exists(kwargs["out_dir"]):
            os.makedirs(kwargs["out_dir"])
        out_path = os.path.join(kwargs["out_dir"], "summary.json")
        with open(out_path, "w") as f:
            json.dump(rv, f)
    else:
        print json.dumps(rv, indent=2)

    if kwargs["meta_dir"]:
        update_wpt_meta(logger_obj, kwargs["meta_dir"], rv)


def get_dir_paths(test_root, test_path):
    if not os.path.isabs(test_path):
        test_path = os.path.join(test_root, test_path)
    dir_path = os.path.dirname(test_path)
    while dir_path != test_root:
        yield os.path.join(dir_path, "__dir__.ini")
        dir_path = os.path.dirname(dir_path)
        assert len(dir_path) >= len(test_root)


def iter_tests(manifests):
    for manifest in manifests.iterkeys():
        for test_type, test_path, tests in manifest:
            url_base = manifests[manifest]["url_base"]
            metadata_base = manifests[manifest]["metadata_path"]
            expected_manifest = get_manifest(metadata_base, test_path, url_base)
            if expected_manifest:
                yield metadata_base, test_path, expected_manifest


def add_manifest(target, path, metadata):
    dir_name = os.path.dirname(path)
    key = [dir_name]

    add_metadata(target, key, metadata)

    key.append("_tests")

    for test_metadata in metadata.children:
        key.append(test_metadata.name)
        add_metadata(target, key, test_metadata)
        key.append("_subtests")
        for subtest_metadata in test_metadata.children:
            key.append(subtest_metadata.name)
            add_metadata(target,
                         key,
                         subtest_metadata)
            key.pop()
        key.pop()
        key.pop()


simple_props = ["disabled", "min-asserts", "max-asserts", "lsan-allowed",
                "leak-allowed", "bug"]
statuses = set(["CRASH"])


def add_metadata(target, key, metadata):
    if not is_interesting(metadata):
        return

    for part in key:
        if part not in target:
            target[part] = {}
        target = target[part]

    for prop in simple_props:
        if metadata.has_key(prop):
            target[prop] = get_condition_value_list(metadata, prop)

    if metadata.has_key("expected"):
        intermittent = []
        values = metadata.get("expected")
        by_status = defaultdict(list)
        for item in values:
            if isinstance(item, tuple):
                condition, status = item
            else:
                condition = None
                status = item
            if isinstance(status, list):
                intermittent.append((condition, status))
                expected_status = status[0]
            else:
                expected_status = status
            by_status[expected_status].append(condition)
        for status in statuses:
            if status in by_status:
                target["expected_%s" % status] = [serialize(item) if item else None
                                                  for item in by_status[status]]
        if intermittent:
            target["intermittent"] = [[serialize(cond) if cond else None, intermittent_statuses]
                                      for cond, intermittent_statuses in intermittent]


def get_condition_value_list(metadata, key):
    conditions = []
    for item in metadata.get(key):
        if isinstance(item, tuple):
            assert len(item) == 2
            conditions.append((serialize(item[0]), item[1]))
        else:
            conditions.append((None, item))
    return conditions


def is_interesting(metadata):
    if any(metadata.has_key(prop) for prop in simple_props):
        return True

    if metadata.has_key("expected"):
        for expected_value in metadata.get("expected"):
            # Include both expected and known intermittent values
            if isinstance(expected_value, tuple):
                expected_value = expected_value[1]
            if isinstance(expected_value, list):
                return True
            if expected_value in statuses:
                return True
            return True
    return False


def update_wpt_meta(logger, meta_root, data):
    global yaml
    import yaml

    if not os.path.exists(meta_root) or not os.path.isdir(meta_root):
        raise ValueError("%s is not a directory" % (meta_root,))

    with WptMetaCollection(meta_root) as wpt_meta:
        for dir_path, dir_data in sorted(data.iteritems()):
            for test, test_data in dir_data.get("_tests", {}).iteritems():
                add_test_data(logger, wpt_meta, dir_path, test, None, test_data)
                for subtest, subtest_data in test_data.get("_subtests", {}).iteritems():
                    add_test_data(logger, wpt_meta, dir_path, test, subtest, subtest_data)

def add_test_data(logger, wpt_meta, dir_path, test, subtest, test_data):
    triage_keys = ["bug"]

    for key in triage_keys:
        if key in test_data:
            value = test_data[key]
            for cond_value in value:
                if cond_value[0] is not None:
                    logger.info("Skipping conditional metadata")
                    continue
                cond_value = cond_value[1]
                if not isinstance(cond_value, list):
                    cond_value = [cond_value]
                for bug_value in cond_value:
                    bug_link = get_bug_link(bug_value)
                    if bug_link is None:
                        logger.info("Could not extract bug: %s" % value)
                        continue
                    meta = wpt_meta.get(dir_path)
                    meta.set(test,
                             subtest,
                             product="firefox",
                             bug_url=bug_link)


bugzilla_re = re.compile("https://bugzilla\.mozilla\.org/show_bug\.cgi\?id=\d+")
bug_re = re.compile("(?:[Bb][Uu][Gg])?\s*(\d+)")


def get_bug_link(value):
    value = value.strip()
    m = bugzilla_re.match(value)
    if m:
        return m.group(0)
    m = bug_re.match(value)
    if m:
        return "https://bugzilla.mozilla.org/show_bug.cgi?id=%s" % m.group(1)


class WptMetaCollection(object):
    def __init__(self, root):
        self.root = root
        self.loaded = {}

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        for item in self.loaded.itervalues():
            item.write(self.root)
        self.loaded = {}

    def get(self, dir_path):
        if dir_path not in self.loaded:
            meta = WptMeta.get_or_create(self.root, dir_path)
            self.loaded[dir_path] = meta
        return self.loaded[dir_path]


class WptMeta(object):
    def __init__(self, dir_path, data):
        assert "links" in data and isinstance(data["links"], list)
        self.dir_path = dir_path
        self.data = data

    @staticmethod
    def meta_path(meta_root, dir_path):
        return os.path.join(meta_root, dir_path, "META.yml")

    def path(self, meta_root):
        return self.meta_path(meta_root, self.dir_path)

    @classmethod
    def get_or_create(cls, meta_root, dir_path):
        if os.path.exists(cls.meta_path(meta_root, dir_path)):
            return cls.load(meta_root, dir_path)
        return cls(dir_path, {"links": []})

    @classmethod
    def load(cls, meta_root, dir_path):
        with open(cls.meta_path(meta_root, dir_path), "r") as f:
            data = yaml.safe_load(f)
        return cls(dir_path, data)

    def set(self, test, subtest, product, bug_url):
        target_link = None
        for link in self.data["links"]:
            link_product = link.get("product")
            if link_product:
                link_product = link_product.split("-", 1)[0]
            if link_product is None or link_product == product:
                if link["url"] == bug_url:
                    target_link = link
                    break

        if target_link is None:
            target_link = {"product": product.encode("utf8"),
                           "url": bug_url.encode("utf8"),
                           "results": []}
            self.data["links"].append(target_link)

        if not "results" in target_link:
            target_link["results"] = []

        has_result = any((result["test"] == test and result.get("subtest") == subtest)
                          for result in target_link["results"])
        if not has_result:
            data = {"test": test.encode("utf8")}
            if subtest:
                data["subtest"] = subtest.encode("utf8")
            target_link["results"].append(data)

    def write(self, meta_root):
        path = self.path(meta_root)
        dirname = os.path.dirname(path)
        if not os.path.exists(dirname):
            os.makedirs(dirname)
        with open(path, "wb") as f:
            yaml.safe_dump(self.data, f,
                           default_flow_style=False,
                           allow_unicode=True)
