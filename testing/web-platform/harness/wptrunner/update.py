# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import sys
import traceback
import uuid

import vcs
from vcs import git, hg
manifest = None
import metadata
import wptcommandline

base_path = os.path.abspath(os.path.split(__file__)[0])

bsd_license = """W3C 3-clause BSD License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of works must retain the original copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the original copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of the W3C nor the names of its contributors may be
  used to endorse or promote products derived from this work without
  specific prior written permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
"""

def do_test_relative_imports(test_root):
    global manifest

    sys.path.insert(0, os.path.join(test_root))
    sys.path.insert(0, os.path.join(test_root, "tools", "scripts"))
    import manifest


class RepositoryError(Exception):
    pass


class WebPlatformTests(object):
    def __init__(self, remote_url, repo_path, rev="origin/master"):
        self.remote_url = remote_url
        self.repo_path = repo_path
        self.target_rev = rev
        self.local_branch = uuid.uuid4().hex

    def update(self):
        if not os.path.exists(self.repo_path):
            os.makedirs(self.repo_path)
        if not vcs.is_git_root(self.repo_path):
            git("clone", self.remote_url, ".", repo=self.repo_path)
            git("checkout", "-b", self.local_branch, self.target_rev, repo=self.repo_path)
            assert vcs.is_git_root(self.repo_path)
        else:
            if git("status", "--porcelain", repo=self.repo_path):
                raise RepositoryError("Repository in %s not clean" % self.repo_path)

            git("fetch",
                self.remote_url,
                "%s:%s" % (self.target_rev,
                           self.local_branch),
                repo=self.repo_path)
            git("checkout", self.local_branch, repo=self.repo_path)
        git("submodule", "init", repo=self.repo_path)
        git("submodule", "update", "--init", "--recursive", repo=self.repo_path)

    @property
    def rev(self):
        if vcs.is_git_root(self.repo_path):
            return git("rev-parse", "HEAD", repo=self.repo_path).strip()
        else:
            return None

    def clean(self):
        git("checkout", self.rev, repo=self.repo_path)
        git("branch", "-D", self.local_branch, repo=self.repo_path)

    def _tree_paths(self):
        repo_paths = [self.repo_path] +  [os.path.join(self.repo_path, path)
                                          for path in self._submodules()]

        rv = []

        for repo_path in repo_paths:
            paths = git("ls-tree", "-r", "--name-only", "HEAD", repo=repo_path).split("\n")
            rel_path = os.path.relpath(repo_path, self.repo_path)
            rv.extend([os.path.join(rel_path, item.strip()) for item in paths if item.strip()])

        return rv

    def _submodules(self):
        output = git("submodule", "status", "--recursive", repo=self.repo_path)
        rv = []
        for line in output.split("\n"):
            line = line.strip()
            if not line:
                continue
            parts = line.split(" ")
            rv.append(parts[1])
        return rv

    def copy_work_tree(self, dest):
        if os.path.exists(dest):
            assert os.path.isdir(dest)

        for sub_path in os.listdir(dest):
            path = os.path.join(dest, sub_path)
            if os.path.isdir(path):
                shutil.rmtree(path)
            else:
                os.remove(path)

        for tree_path in self._tree_paths():
            source_path = os.path.join(self.repo_path, tree_path)
            dest_path = os.path.join(dest, tree_path)

            dest_dir = os.path.split(dest_path)[0]
            if not os.path.isdir(source_path):
                if not os.path.exists(dest_dir):
                    os.makedirs(dest_dir)
                shutil.copy2(source_path, dest_path)

        for source, destination in [("testharness_runner.html", ""),
                                    ("testharnessreport.js", "resources/")]:
            source_path = os.path.join(base_path, source)
            dest_path = os.path.join(dest, destination, os.path.split(source)[1])
            shutil.copy2(source_path, dest_path)

        self.add_license(dest)

    def add_license(self, dest):
        with open(os.path.join(dest, "LICENSE"), "w") as f:
            f.write(bsd_license)


class NoVCSTree(object):
    name = "non-vcs"

    def __init__(self, root=None):
        if root is None:
            root = os.path.abspath(os.curdir)
        self.root = root

    @classmethod
    def is_type(cls, path):
        return True

    def is_clean(self):
        return True

    def add_new(self, prefix=None):
        pass

    def create_patch(self, patch_name, message):
        pass

    def update_patch(self, include=None):
        pass

    def commit_patch(self):
        pass


class HgTree(object):
    name = "mercurial"

    def __init__(self, root=None):
        if root is None:
            root = hg("root").strip()
        self.root = root
        self.hg = vcs.bind_to_repo(hg, self.root)

    @classmethod
    def is_type(cls, path):
        try:
            hg("root", repo=path)
        except:
            return False
        return True

    def is_clean(self):
        return self.hg("status").strip() == ""

    def add_new(self, prefix=None):
        if prefix is not None:
            args = ("-I", prefix)
        else:
            args = ()
        self.hg("add", *args)

    def create_patch(self, patch_name, message):
        try:
            self.hg("qinit")
        except subprocess.CalledProcessError:
            pass

        patch_names = [item.strip() for item in self.hg("qseries").split("\n") if item.strip()]

        suffix = 0
        test_name = patch_name
        while test_name in patch_names:
            suffix += 1
            test_name = "%s-%i" % (patch_name, suffix)

        self.hg("qnew", test_name, "-X", self.root, "-m", message)

    def update_patch(self, include=None):
        if include is not None:
            args = []
            for item in include:
                args.extend(["-I", item])
        else:
            args = ()

        self.hg("qrefresh", *args)

    def commit_patch(self):
        self.hg("qfinish", repo=self.repo_root)


class GitTree(object):
    name = "git"

    def __init__(self, root=None):
        if root is None:
            root = git("rev-parse", "--show-toplevel").strip()
        self.root = root
        self.git = vcs.bind_to_repo(git, self.root)
        self.message = None

    @classmethod
    def is_type(cls, path):
        try:
            git("rev-parse", "--show-toplevel", repo=path)
        except:
            return False
        return True

    def is_clean(self):
        return self.git("status").strip() == ""

    def add_new(self, prefix=None):
        if prefix is None:
            args = ("-a",)
        else:
            args = ("--no-ignore-removal", prefix)
        self.git("add", *args)

    def create_patch(self, patch_name, message):
        # In git a patch is actually a commit
        self.message = message

    def update_patch(self, include=None):
        assert self.message is not None

        if include is not None:
            args = tuple(include)
        else:
            args = ()

        self.git("commit", "-m", self.message, *args)

    def commit_patch(self):
        pass


class Runner(object):
    def __init__(self, bug, **kwargs):
        self.bug = bug
        for k, v in kwargs.iteritems():
            setattr(self, k, v)

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        self.cleanup()

    def cleanup(self):
        pass


def ensure_exists(path):
    if not os.path.exists(path):
        os.makedirs(path)


def sync_tests(paths, local_tree, wpt, bug):
    wpt.update()

    try:
        #bug.comment("Updating to %s" % wpt.rev)
        initial_manifest = metadata.load_test_manifest(paths["sync"], paths["metadata"])
        wpt.copy_work_tree(paths["test"])
        new_manifest = metadata.update_manifest(paths["sync"], paths["metadata"])

        local_tree.create_patch("web-platform-tests_update_%s" % wpt.rev,
                                "Update web-platform-tests to revision %s" % wpt.rev)
        local_tree.add_new(os.path.relpath(paths["test"], local_tree.root))
        local_tree.update_patch(include=[paths["test"], paths["metadata"]])
    except Exception as e:
        #bug.comment("Update failed with error:\n %s" % traceback.format_exc())
        sys.stderr.write(traceback.format_exc())
        raise
    finally:
        pass  # wpt.clean()

    return initial_manifest, new_manifest


def update_metadata(paths, local_tree, initial_rev, bug, log_files, ignore_existing,
                    wpt_repo=None):
    try:
        try:
            if wpt_repo is not None:
                name = "web-platform-tests_update_%s_metadata" % wpt_repo.rev
                message = "Update web-platform-tests expected data to revision %s" % wpt_repo.rev
            else:
                name = "web-platform-tests_update_metadata"
                message = "Update web-platform-tests expected data"

            local_tree.create_patch(name, message)
        except subprocess.CalledProcessError:
            # Patch with that name already exists, probably
            pass
        needs_human = metadata.update_expected(paths["test"],
                                               paths["metadata"],
                                               log_files,
                                               rev_old=initial_rev,
                                               ignore_existing=ignore_existing,
                                               sync_root=paths.get("sync", None))

        if needs_human:
            #TODO: List all the files that should be checked carefully for changes.
            pass

        if not local_tree.is_clean():
            local_tree.add_new(os.path.relpath(paths["metadata"], local_tree.root))
            local_tree.update_patch(include=[paths["metadata"]])

    except Exception as e:
        #bug.comment("Update failed with error:\n %s" % traceback.format_exc())
        sys.stderr.write(traceback.format_exc())
        raise


def run_update(**kwargs):
    config = kwargs["config"]

    paths = {"test": kwargs["tests_root"],
             "metadata": kwargs["metadata_root"]}

    if kwargs["sync"]:
        paths["sync"] = kwargs["sync_path"]

    for path in paths.itervalues():
        ensure_exists(path)

    if not kwargs["sync"] and not kwargs["run_log"]:
        print """Nothing to do.

Specify --sync to checkout latest upstream or one or more log files to update
expected data."""

    if kwargs["patch"]:
        for tree_cls in [HgTree, GitTree, NoVCSTree]:
            if tree_cls.is_type(os.path.abspath(os.curdir)):
                local_tree = tree_cls()
                print "Updating into a %s tree" % local_tree.name
                break
    else:
        local_tree = NoVCSTree()

    if not local_tree.is_clean():
        sys.stderr.write("Working tree is not clean\n")
        if not kwargs["no_check_clean"]:
            sys.exit(1)

    rev = kwargs.get("rev")
    if rev is None:
        rev = config["web-platform-tests"].get("branch", "master")

    bug = None

    initial_rev = None
    wpt_repo = None

    if kwargs["sync"]:
        wpt_repo = WebPlatformTests(config["web-platform-tests"]["remote_url"],
                                    paths["sync"],
                                    rev=rev)
        initial_manifest, new_manifest = sync_tests(paths, local_tree, wpt_repo, bug)
        initial_rev = initial_manifest.rev

    if kwargs["run_log"]:
        update_metadata(paths,
                        local_tree,
                        initial_rev,
                        bug,
                        kwargs["run_log"],
                        kwargs["ignore_existing"],
                        wpt_repo=wpt_repo)


def main():
    args = wptcommandline.parse_args_update()
    success = run_update(**args)
    sys.exit(0 if success else 1)
