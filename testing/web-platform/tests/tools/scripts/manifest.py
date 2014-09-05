#!/usr/bin/env python

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import urlparse

from collections import defaultdict
from fnmatch import fnmatch


def get_git_func(repo_path):
    def git(cmd, *args):
        full_cmd = ["git", cmd] + list(args)
        return subprocess.check_output(full_cmd, cwd=repo_path, stderr=subprocess.STDOUT)
    return git


def setup_git(repo_path):
    assert os.path.exists(os.path.join(repo_path, ".git"))
    global git
    git = get_git_func(repo_path)


_repo_root = None
def get_repo_root():
    global _repo_root
    if _repo_root is None:
        git = get_git_func(os.path.dirname(__file__))
        _repo_root = git("rev-parse", "--show-toplevel").rstrip()
    return _repo_root


manifest_name = "MANIFEST.json"
exclude_php_hack = True
ref_suffixes = ["_ref", "-ref"]
wd_pattern = "*.py"
blacklist = ["/", "/tools/", "/resources/", "/common/", "/conformance-checkers/"]

logging.basicConfig()
logger = logging.getLogger("manifest")
logger.setLevel(logging.DEBUG)


class ManifestItem(object):
    item_type = None

    def __init__(self, path):
        self.path = path

    def _key(self):
        return self.item_type, self.path

    def __eq__(self, other):
        if not hasattr(other, "_key"):
            return False
        return self._key() == other._key()

    def __hash__(self):
        return hash(self._key())

    def to_json(self):
        return {"path": self.path}

    @classmethod
    def from_json(self, obj):
        raise NotImplementedError

    @property
    def id(self):
        raise NotImplementedError

class TestharnessTest(ManifestItem):
    item_type = "testharness"

    def __init__(self, path, url, timeout=None):
        ManifestItem.__init__(self, path)
        self.url = url
        self.timeout = timeout

    @property
    def id(self):
        return self.url

    def to_json(self):
        rv = ManifestItem.to_json(self)
        rv.update({"url": self.url})
        if self.timeout:
            rv["timeout"] = self.timeout
        return rv

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"],
                   obj["url"],
                   timeout=obj.get("timeout"))


class RefTest(ManifestItem):
    item_type = "reftest"

    def __init__(self, path, url, ref_url, ref_type,
                 timeout=None):
        if ref_type not in ["==", "!="]:
            raise ValueError, "Unrecognised ref_type %s" % ref_type
        ManifestItem.__init__(self, path)
        self.url = url
        self.ref_url = ref_url
        self.ref_type = ref_type
        self.timeout = timeout

    @property
    def id(self):
        return (self.url, self.ref_type, self.ref_url)

    def _key(self):
        return self.item_type, self.url, self.ref_type, self.ref_url

    def to_json(self):
        rv = ManifestItem.to_json(self)
        rv.update({"url": self.url,
                   "ref_type": self.ref_type,
                   "ref_url": self.ref_url})
        if self.timeout:
            rv["timeout"] = self.timeout
        return rv

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"], obj["url"], obj["ref_url"], obj["ref_type"],
                   timeout=obj.get("timeout"))


class ManualTest(ManifestItem):
    item_type = "manual"

    def __init__(self, path, url):
        ManifestItem.__init__(self, path)
        self.url = url

    @property
    def id(self):
        return self.url

    def to_json(self):
        rv = ManifestItem.to_json(self)
        rv.update({"url": self.url})
        return rv

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"], obj["url"])


class Stub(ManifestItem):
    item_type = "stub"

    def __init__(self, path, url):
        ManifestItem.__init__(self, path)
        self.url = url

    @property
    def id(self):
        return self.url

    def to_json(self):
        rv = ManifestItem.to_json(self)
        rv.update({"url": self.url})
        return rv

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"], obj["url"])


class Helper(ManifestItem):
    item_type = "helper"

    def __init__(self, path, url):
        ManifestItem.__init__(self, path)
        self.url = url

    @property
    def id(self):
        return self.url

    def to_json(self):
        rv = ManifestItem.to_json(self)
        rv.update({"url": self.url})
        return rv

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"], obj["url"])


class WebdriverSpecTest(ManifestItem):
    item_type = "wdspec"

    @property
    def id(self):
        return self.path

    @classmethod
    def from_json(cls, obj):
        return cls(obj["path"])


class ManifestError(Exception):
    pass


class Manifest(object):
    def __init__(self, git_rev):
        self.item_types = [
            "testharness", "reftest", "manual", "helper", "stub", "wdspec"]
        self._data = dict((item_type, defaultdict(set))
                          for item_type in self.item_types)
        self.rev = git_rev
        self.local_changes = LocalChanges()

    def contains_path(self, path):
        return any(path in item for item in self._data.itervalues())

    def add(self, item):
        self._data[item.item_type][item.path].add(item)

    def extend(self, items):
        for item in items:
            self.add(item)

    def remove_path(self, path):
        for item_type in self.item_types:
            if path in self._data[item_type]:
                del self._data[item_type][path]

    def itertypes(self, *types):
        for item_type in types:
            for item in sorted(self._data[item_type].items()):
                yield item

    def __iter__(self):
        for item_type in self.item_types:
            for item in self._data[item_type].iteritems():
                yield item

    def __getitem__(self, key):
        for items in self._data.itervalues():
            if key in items:
                return items[key]
        raise KeyError

    def to_json(self):
        items = defaultdict(list)
        for test_path, tests in self.itertypes(*self.item_types):
            for test in tests:
                items[test.item_type].append(test.to_json())

        rv = {"rev":self.rev,
              "local_changes":self.local_changes.to_json(),
              "items":items}
        return rv

    @classmethod
    def from_json(cls, obj):
        self = cls(obj["rev"])
        if not hasattr(obj, "iteritems"):
            raise ManifestError

        item_classes = {"testharness":TestharnessTest,
                        "reftest":RefTest,
                        "manual":ManualTest,
                        "helper":Helper,
                        "stub": Stub,
                        "wdspec": WebdriverSpecTest}

        for k, values in obj["items"].iteritems():
            if k not in self.item_types:
                raise ManifestError
            for v in values:
                manifest_item = item_classes[k].from_json(v)
                self.add(manifest_item)
        self.local_changes = LocalChanges.from_json(obj["local_changes"])
        return self


class LocalChanges(dict):
    def __setitem__(self, path, status):
        if status not in ["A", "M", "D"]:
            raise ValueError, "Unrecognised status %s for path %s" % (status, path)
        dict.__setitem__(self, path, status)

    def to_json(self):
        return [(self[item], item) for item in sorted(self.keys())]

    @classmethod
    def from_json(cls, obj):
        self = cls()
        for status, path in obj:
            self[path] = status
        return self


def get_ref(path):
    base_path, filename = os.path.split(path)
    name, ext = os.path.splitext(filename)
    for suffix in ref_suffixes:
        possible_ref = os.path.join(base_path, name + suffix + ext)
        if os.path.exists(possible_ref):
            return possible_ref


def markup_type(ext):
    if not ext:
        return None
    if ext[0] == ".":
        ext = ext[1:]
    if ext in ["html", "htm"]:
        return "html"
    elif ext in ["xhtml", "xht"]:
        return "xhtml"
    elif ext == "svg":
        return "svg"
    return None


def get_timeout_meta(root):
    return root.findall(".//{http://www.w3.org/1999/xhtml}meta[@name='timeout']")


def get_testharness_scripts(root):
    return root.findall(".//{http://www.w3.org/1999/xhtml}script[@src='/resources/testharness.js']")


def get_reference_links(root):
    match_links = root.findall(".//{http://www.w3.org/1999/xhtml}link[@rel='match']")
    mismatch_links = root.findall(".//{http://www.w3.org/1999/xhtml}link[@rel='mismatch']")
    return match_links, mismatch_links


def get_manifest_items(rel_path):
    if rel_path.endswith(os.path.sep):
        return []

    url = "/" + rel_path.replace(os.sep, "/")

    path = os.path.join(get_repo_root(), rel_path)
    if not os.path.exists(path):
        return []

    base_path, filename = os.path.split(path)
    name, ext = os.path.splitext(filename)
    rel_dir_tree = rel_path.split(os.path.sep)

    file_markup_type = markup_type(ext)

    if filename.startswith("MANIFEST") or filename.startswith("."):
        return []

    for item in blacklist:
        if item == "/":
            if "/" not in url[1:]:
                return []
        elif url.startswith(item):
            return []

    if name.startswith("stub-"):
        return [Stub(rel_path, url)]

    if name.lower().endswith("-manual"):
        return [ManualTest(rel_path, url)]

    ref_list = []

    for suffix in ref_suffixes:
        if name.endswith(suffix):
            return [Helper(rel_path, rel_path)]
        elif os.path.exists(os.path.join(base_path, name + suffix + ext)):
            ref_url, ref_ext = url.rsplit(".", 1)
            ref_url = ref_url + suffix + ext
            #Need to check if this is the right reftype
            ref_list = [RefTest(rel_path, url, ref_url, "==")]

    # wdspec tests are in subdirectories of /webdriver excluding __init__.py
    # files.
    if (rel_dir_tree[0] == "webdriver" and
        len(rel_dir_tree) > 2 and
        filename != "__init__.py" and
        fnmatch(filename, wd_pattern)):
        return [WebdriverSpecTest(rel_path)]

    if file_markup_type:
        timeout = None

        if exclude_php_hack:
            php_re =re.compile("\.php")
            with open(path) as f:
                text = f.read()
                if php_re.findall(text):
                    return []

        parser = {"html":lambda x:html5lib.parse(x, treebuilder="etree"),
                  "xhtml":ElementTree.parse,
                  "svg":ElementTree.parse}[file_markup_type]
        try:
            with open(path) as f:
                tree = parser(f)
        except:
            return [Helper(rel_path, url)]

        if hasattr(tree, "getroot"):
            root = tree.getroot()
        else:
            root = tree

        timeout_nodes = get_timeout_meta(root)
        if timeout_nodes:
            timeout_str = timeout_nodes[0].attrib.get("content", None)
            if timeout_str and timeout_str.lower() == "long":
                try:
                    timeout = timeout_str.lower()
                except:
                    pass

        if get_testharness_scripts(root):
            return [TestharnessTest(rel_path, url, timeout=timeout)]
        else:
            match_links, mismatch_links = get_reference_links(root)
            for item in match_links + mismatch_links:
                ref_url = urlparse.urljoin(url, item.attrib["href"])
                ref_type = "==" if item.attrib["rel"] == "match" else "!="
                reftest = RefTest(rel_path, url, ref_url, ref_type, timeout=timeout)
                if reftest not in ref_list:
                    ref_list.append(reftest)
            return ref_list

    return [Helper(rel_path, url)]


def abs_path(path):
    return os.path.abspath(path)


def get_repo_paths():
    data = git("ls-tree", "--name-only", "--full-tree", "-r", "HEAD")
    return [item for item in data.split("\n") if not item.endswith(os.path.sep)]


def get_committed_changes(base_rev):
    if base_rev is None:
        logger.debug("Adding all changesets to the manifest")
        return [("A", item) for item in get_repo_paths()]
    else:
        logger.debug("Updating the manifest from %s to %s" % (base_rev, get_current_rev()))
        data  = git("diff", "--name-status", base_rev)
        return [line.split("\t", 1) for line in data.split("\n") if line]


def has_local_changes():
    return git("status", "--porcelain", "--ignore-submodules=untracked").strip() != ""


def get_local_changes():
    #This doesn't account for whole directories that have been added
    data  = git("status", "--porcelain", "--ignore-submodules=all")
    rv = LocalChanges()
    for line in data.split("\n"):
        line = line.strip()
        if not line:
            continue
        status, path = line.split(" ", 1)
        if path.endswith(os.path.sep):
            logger.warning("Ignoring added directory %s" % path)
            continue
        if status == "??":
            status = "A"
        elif status == "R":
            old_path, path = tuple(item.strip() for item in path.split("->"))
            rv[old_path] = "D"
            status = "A"
        elif status == "MM":
            status = "M"
        rv[path] = status
    return rv


def sync_urls(manifest, updated_files):
    for status, path in updated_files:
        if status in ("D", "M"):
            manifest.remove_path(path)
        if status in ("A", "M"):
            manifest.extend(get_manifest_items(path))


def sync_local_changes(manifest, local_changes):
    if local_changes:
        logger.info("Working directory not clean, adding local changes")
    prev_local_changes = manifest.local_changes
    all_paths = get_repo_paths()

    for path, status in prev_local_changes.iteritems():
        print status, path, path in local_changes
        if path not in local_changes:
            # If a path was previously marked as deleted but is now back
            # we need to readd it to the manifest
            if status == "D" and path in all_paths:
                local_changes[path] = "A"
            # If a path was previously marked as added but is now
            # not then we need to remove it from the manifest
            elif status == "A" and path not in all_paths:
                local_changes[path] = "D"

    sync_urls(manifest, ((status, path) for path, status in local_changes.iteritems()))


def get_current_rev():
    return git("rev-parse", "HEAD").strip()


def load(manifest_path):
    if os.path.exists(manifest_path):
        logger.debug("Opening manifest at %s" % manifest_path)
    else:
        logger.debug("Creating new manifest at %s" % manifest_path)
    try:
        with open(manifest_path) as f:
            manifest = Manifest.from_json(json.load(f))
    except IOError as e:
        manifest = Manifest(None)

    return manifest


def update(manifest):
    global ElementTree
    global html5lib

    try:
        from xml.etree import cElementTree as ElementTree
    except ImportError:
        from xml.etree import ElementTree

    import html5lib

    sync_urls(manifest, get_committed_changes(manifest.rev))
    sync_local_changes(manifest, get_local_changes())

    manifest.rev = get_current_rev()


def write(manifest, manifest_path):
    with open(manifest_path, "w") as f:
        json.dump(manifest.to_json(), f, sort_keys=True, indent=2, separators=(',', ': '))


def update_manifest(repo_path, **kwargs):
    setup_git(repo_path)
    if not kwargs.get("rebuild", False):
        manifest = load(kwargs["path"])
    else:
        manifest = Manifest(None)
    if has_local_changes() and not kwargs.get("local_changes", False):
        logger.info("Not writing manifest because working directory is not clean.")
    else:
        logger.info("Updating manifest")
        update(manifest)
        write(manifest, kwargs["path"])


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--path", default=os.path.join(get_repo_root(), "MANIFEST.json"),
        help="path to manifest file")
    parser.add_argument(
        "-r", "--rebuild", action="store_true", default=False,
        help="force a full rebuild of the manifest rather than updating "
        "incrementally")
    parser.add_argument(
        "-c", "--experimental-include-local-changes", action="store_true", default=False,
        help="include local changes in the manifest rather than just committed "
             "changes (experimental)")
    return parser

if __name__ == "__main__":
    try:
        get_repo_root()
    except subprocess.CalledProcessError:
        print "Script must be inside a web-platform-tests git clone."
        sys.exit(1)
    opts = create_parser().parse_args()
    update_manifest(get_repo_root(),
                    path=opts.path,
                    rebuild=opts.rebuild,
                    local_changes=opts.experimental_include_local_changes)
