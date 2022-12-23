import os
import platform
import shutil
import tempfile
import unittest

import mozharness.base.vcs.mercurial as mercurial

test_string = """foo
bar
baz"""

HG = ["hg"] + mercurial.HG_OPTIONS

# Known default .hgrc
os.environ["HGRCPATH"] = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "helper_files", ".hgrc")
)


def cleanup():
    if os.path.exists("test_logs"):
        shutil.rmtree("test_logs")
    if os.path.exists("test_dir"):
        if os.path.isdir("test_dir"):
            shutil.rmtree("test_dir")
        else:
            os.remove("test_dir")
    for filename in ("localconfig.json", "localconfig.json.bak"):
        if os.path.exists(filename):
            os.remove(filename)


def get_mercurial_vcs_obj():
    m = mercurial.MercurialVCS()
    m.config = {}
    return m


def get_revisions(dest):
    m = get_mercurial_vcs_obj()
    retval = []
    command = HG + ["log", "-R", dest, "--template", "{node}\n"]
    for rev in m.get_output_from_command(command).split("\n"):
        rev = rev.strip()
        if not rev:
            continue
        retval.append(rev)
    return retval


class TestMakeAbsolute(unittest.TestCase):
    # _make_absolute() doesn't play nicely with windows/msys paths.
    # TODO: fix _make_absolute, write it out of the picture, or determine
    # that it's not needed on windows.
    if platform.system() not in ("Windows",):

        def test_absolute_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEqual(m._make_absolute("/foo/bar"), "/foo/bar")

        def test_relative_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEqual(m._make_absolute("foo/bar"), os.path.abspath("foo/bar"))

        def test_HTTP_paths(self):
            m = get_mercurial_vcs_obj()
            self.assertEqual(m._make_absolute("http://foo/bar"), "http://foo/bar")

        def test_absolute_file_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEqual(m._make_absolute("file:///foo/bar"), "file:///foo/bar")

        def test_relative_file_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEqual(
                m._make_absolute("file://foo/bar"), "file://%s/foo/bar" % os.getcwd()
            )


class TestHg(unittest.TestCase):
    def _init_hg_repo(self, hg_obj, repodir):
        hg_obj.run_command(
            [
                "bash",
                os.path.join(
                    os.path.dirname(__file__), "helper_files", "init_hgrepo.sh"
                ),
                repodir,
            ]
        )

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.repodir = os.path.join(self.tmpdir, "repo")
        m = get_mercurial_vcs_obj()
        self._init_hg_repo(m, self.repodir)
        self.revisions = get_revisions(self.repodir)
        self.wc = os.path.join(self.tmpdir, "wc")
        self.pwd = os.getcwd()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)
        os.chdir(self.pwd)

    def test_get_branch(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        b = m.get_branch_from_path(self.wc)
        self.assertEqual(b, "default")

    def test_get_branches(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        branches = m.get_branches_from_path(self.wc)
        self.assertEqual(sorted(branches), sorted(["branch2", "default"]))

    def test_clone(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=False)
        self.assertEqual(rev, None)
        self.assertEqual(self.revisions, get_revisions(self.wc))
        self.assertEqual(sorted(os.listdir(self.wc)), [".hg"])

    def test_clone_into_non_empty_dir(self):
        m = get_mercurial_vcs_obj()
        m.mkdir_p(self.wc)
        open(os.path.join(self.wc, "test.txt"), "w").write("hello")
        m.clone(self.repodir, self.wc, update_dest=False)
        self.assertTrue(not os.path.exists(os.path.join(self.wc, "test.txt")))

    def test_clone_update(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=True)
        self.assertEqual(rev, self.revisions[0])

    def test_clone_branch(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, branch="branch2", update_dest=False)
        # On hg 1.6, we should only have a subset of the revisions
        if m.hg_ver() >= (1, 6, 0):
            self.assertEqual(self.revisions[1:], get_revisions(self.wc))
        else:
            self.assertEqual(self.revisions, get_revisions(self.wc))

    def test_clone_update_branch(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(
            self.repodir,
            os.path.join(self.tmpdir, "wc"),
            branch="branch2",
            update_dest=True,
        )
        self.assertEqual(rev, self.revisions[1], self.revisions)

    def test_clone_revision(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[0], update_dest=False)
        # We'll only get a subset of the revisions
        self.assertEqual(
            self.revisions[:1] + self.revisions[2:], get_revisions(self.wc)
        )

    def test_update_revision(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=False)
        self.assertEqual(rev, None)

        rev = m.update(self.wc, revision=self.revisions[1])
        self.assertEqual(rev, self.revisions[1])

    def test_pull(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEqual(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in new changes
        rev = m.pull(self.repodir, self.wc, update_dest=False)
        self.assertEqual(rev, None)
        self.assertEqual(get_revisions(self.wc), self.revisions)

    def test_pull_revision(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEqual(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in just the last revision
        rev = m.pull(
            self.repodir, self.wc, revision=self.revisions[0], update_dest=False
        )
        self.assertEqual(rev, None)

        # We'll be missing the middle revision (on another branch)
        self.assertEqual(
            get_revisions(self.wc), self.revisions[:1] + self.revisions[2:]
        )

    def test_pull_branch(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEqual(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in the other branch
        rev = m.pull(self.repodir, self.wc, branch="branch2", update_dest=False)
        self.assertEqual(rev, None)

        # On hg 1.6, we'll be missing the last revision (on another branch)
        if m.hg_ver() >= (1, 6, 0):
            self.assertEqual(get_revisions(self.wc), self.revisions[1:])
        else:
            self.assertEqual(get_revisions(self.wc), self.revisions)

    def test_pull_unrelated(self):
        m = get_mercurial_vcs_obj()
        # Create a new repo
        repo2 = os.path.join(self.tmpdir, "repo2")
        self._init_hg_repo(m, repo2)

        self.assertNotEqual(self.revisions, get_revisions(repo2))

        # Clone the original repo
        m.clone(self.repodir, self.wc, update_dest=False)
        # Hide the wanted error
        m.config = {"log_to_console": False}
        # Try and pull in changes from the new repo
        self.assertRaises(
            mercurial.VCSException, m.pull, repo2, self.wc, update_dest=False
        )

    def test_push(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[-2])
        m.push(src=self.repodir, remote=self.wc)
        self.assertEqual(get_revisions(self.wc), self.revisions)

    def test_push_with_branch(self):
        m = get_mercurial_vcs_obj()
        if m.hg_ver() >= (1, 6, 0):
            m.clone(self.repodir, self.wc, revision=self.revisions[-1])
            m.push(src=self.repodir, remote=self.wc, branch="branch2")
            m.push(src=self.repodir, remote=self.wc, branch="default")
            self.assertEqual(get_revisions(self.wc), self.revisions)

    def test_push_with_revision(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[-2])
        m.push(src=self.repodir, remote=self.wc, revision=self.revisions[-1])
        self.assertEqual(get_revisions(self.wc), self.revisions[-2:])

    def test_mercurial(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {
            "repo": self.repodir,
            "dest": self.wc,
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        m.ensure_repo_and_revision()
        rev = m.ensure_repo_and_revision()
        self.assertEqual(rev, self.revisions[0])

    def test_push_new_branches_not_allowed(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[0])
        # Hide the wanted error
        m.config = {"log_to_console": False}
        self.assertRaises(
            Exception, m.push, self.repodir, self.wc, push_new_branches=False
        )

    def test_mercurial_relative_dir(self):
        m = get_mercurial_vcs_obj()
        repo = os.path.basename(self.repodir)
        wc = os.path.basename(self.wc)
        m.vcs_config = {
            "repo": repo,
            "dest": wc,
            "revision": self.revisions[-1],
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        m.chdir(os.path.dirname(self.repodir))
        try:
            rev = m.ensure_repo_and_revision()
            self.assertEqual(rev, self.revisions[-1])
            m.info("Creating test.txt")
            open(os.path.join(self.wc, "test.txt"), "w").write("hello!")

            m = get_mercurial_vcs_obj()
            m.vcs_config = {
                "repo": repo,
                "dest": wc,
                "revision": self.revisions[0],
                "vcs_share_base": os.path.join(self.tmpdir, "share"),
            }
            rev = m.ensure_repo_and_revision()
            self.assertEqual(rev, self.revisions[0])
            # Make sure our local file didn't go away
            self.assertTrue(os.path.exists(os.path.join(self.wc, "test.txt")))
        finally:
            m.chdir(self.pwd)

    def test_mercurial_update_tip(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {
            "repo": self.repodir,
            "dest": self.wc,
            "revision": self.revisions[-1],
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        rev = m.ensure_repo_and_revision()
        self.assertEqual(rev, self.revisions[-1])
        open(os.path.join(self.wc, "test.txt"), "w").write("hello!")

        m = get_mercurial_vcs_obj()
        m.vcs_config = {
            "repo": self.repodir,
            "dest": self.wc,
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        rev = m.ensure_repo_and_revision()
        self.assertEqual(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.assertTrue(os.path.exists(os.path.join(self.wc, "test.txt")))

    def test_mercurial_update_rev(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {
            "repo": self.repodir,
            "dest": self.wc,
            "revision": self.revisions[-1],
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        rev = m.ensure_repo_and_revision()
        self.assertEqual(rev, self.revisions[-1])
        open(os.path.join(self.wc, "test.txt"), "w").write("hello!")

        m = get_mercurial_vcs_obj()
        m.vcs_config = {
            "repo": self.repodir,
            "dest": self.wc,
            "revision": self.revisions[0],
            "vcs_share_base": os.path.join(self.tmpdir, "share"),
        }
        rev = m.ensure_repo_and_revision()
        self.assertEqual(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.assertTrue(os.path.exists(os.path.join(self.wc, "test.txt")))

    def test_make_hg_url(self):
        # construct an hg url specific to revision, branch and filename and try to pull it down
        file_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "//build/tools/",
            revision="FIREFOX_3_6_12_RELEASE",
            filename="/lib/python/util/hg.py",
            protocol="https",
        )
        expected_url = (
            "https://hg.mozilla.org/build/tools/raw-file/"
            "FIREFOX_3_6_12_RELEASE/lib/python/util/hg.py"
        )
        self.assertEqual(file_url, expected_url)

    def test_make_hg_url_no_filename(self):
        file_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            revision="default",
            protocol="https",
        )
        expected_url = "https://hg.mozilla.org/build/tools/rev/default"
        self.assertEqual(file_url, expected_url)

    def test_make_hg_url_no_revision_no_filename(self):
        repo_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            protocol="https",
        )
        expected_url = "https://hg.mozilla.org/build/tools"
        self.assertEqual(repo_url, expected_url)

    def test_make_hg_url_different_protocol(self):
        repo_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            protocol="ssh",
        )
        expected_url = "ssh://hg.mozilla.org/build/tools"
        self.assertEqual(repo_url, expected_url)


if __name__ == "__main__":
    unittest.main()
