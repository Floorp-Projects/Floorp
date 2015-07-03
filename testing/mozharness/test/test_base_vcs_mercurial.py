import os
import platform
import shutil
import tempfile
import unittest

import mozharness.base.errors as errors
import mozharness.base.vcs.mercurial as mercurial

test_string = '''foo
bar
baz'''

HG = ['hg'] + mercurial.HG_OPTIONS

# Known default .hgrc
os.environ['HGRCPATH'] = os.path.join(os.path.dirname(__file__), 'helper_files', '.hgrc')


def cleanup():
    if os.path.exists('test_logs'):
        shutil.rmtree('test_logs')
    if os.path.exists('test_dir'):
        if os.path.isdir('test_dir'):
            shutil.rmtree('test_dir')
        else:
            os.remove('test_dir')
    for filename in ('localconfig.json', 'localconfig.json.bak'):
        if os.path.exists(filename):
            os.remove(filename)


def get_mercurial_vcs_obj():
    m = mercurial.MercurialVCS()
    m.config = {}
    return m


def get_revisions(dest):
    m = get_mercurial_vcs_obj()
    retval = []
    for rev in m.get_output_from_command(HG + ['log', '-R', dest, '--template', '{node|short}\n']).split('\n'):
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
            self.assertEquals(m._make_absolute("/foo/bar"), "/foo/bar")

        def test_relative_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEquals(m._make_absolute("foo/bar"), os.path.abspath("foo/bar"))

        def test_HTTP_paths(self):
            m = get_mercurial_vcs_obj()
            self.assertEquals(m._make_absolute("http://foo/bar"), "http://foo/bar")

        def test_absolute_file_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEquals(m._make_absolute("file:///foo/bar"), "file:///foo/bar")

        def test_relative_file_path(self):
            m = get_mercurial_vcs_obj()
            self.assertEquals(m._make_absolute("file://foo/bar"), "file://%s/foo/bar" % os.getcwd())


class TestHg(unittest.TestCase):
    def _init_hg_repo(self, hg_obj, repodir):
        hg_obj.run_command(["bash",
                            os.path.join(os.path.dirname(__file__),
                                         "helper_files", "init_hgrepo.sh"),
                            repodir])

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.repodir = os.path.join(self.tmpdir, 'repo')
        m = get_mercurial_vcs_obj()
        self._init_hg_repo(m, self.repodir)
        self.revisions = get_revisions(self.repodir)
        self.wc = os.path.join(self.tmpdir, 'wc')
        self.pwd = os.getcwd()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)
        os.chdir(self.pwd)

    def test_get_branch(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        b = m.get_branch_from_path(self.wc)
        self.assertEquals(b, 'default')

    def test_get_branches(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        branches = m.get_branches_from_path(self.wc)
        self.assertEquals(sorted(branches), sorted(["branch2", "default"]))

    def test_clone(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)
        self.assertEquals(self.revisions, get_revisions(self.wc))
        self.assertEquals(sorted(os.listdir(self.wc)), ['.hg'])

    def test_clone_into_non_empty_dir(self):
        m = get_mercurial_vcs_obj()
        m.mkdir_p(self.wc)
        open(os.path.join(self.wc, 'test.txt'), 'w').write('hello')
        m.clone(self.repodir, self.wc, update_dest=False)
        self.failUnless(not os.path.exists(os.path.join(self.wc, 'test.txt')))

    def test_clone_update(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=True)
        self.assertEquals(rev, self.revisions[0])

    def test_clone_branch(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, branch='branch2',
                update_dest=False)
        # On hg 1.6, we should only have a subset of the revisions
        if m.hg_ver() >= (1, 6, 0):
            self.assertEquals(self.revisions[1:],
                              get_revisions(self.wc))
        else:
            self.assertEquals(self.revisions,
                              get_revisions(self.wc))

    def test_clone_update_branch(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, os.path.join(self.tmpdir, 'wc'),
                      branch="branch2", update_dest=True)
        self.assertEquals(rev, self.revisions[1], self.revisions)

    def test_clone_revision(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc,
                revision=self.revisions[0], update_dest=False)
        # We'll only get a subset of the revisions
        self.assertEquals(self.revisions[:1] + self.revisions[2:],
                          get_revisions(self.wc))

    def test_update_revision(self):
        m = get_mercurial_vcs_obj()
        rev = m.clone(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)

        rev = m.update(self.wc, revision=self.revisions[1])
        self.assertEquals(rev, self.revisions[1])

    def test_pull(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEquals(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in new changes
        rev = m.pull(self.repodir, self.wc, update_dest=False)
        self.assertEquals(rev, None)
        self.assertEquals(get_revisions(self.wc), self.revisions)

    def test_pull_revision(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEquals(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in just the last revision
        rev = m.pull(self.repodir, self.wc, revision=self.revisions[0], update_dest=False)
        self.assertEquals(rev, None)

        # We'll be missing the middle revision (on another branch)
        self.assertEquals(get_revisions(self.wc), self.revisions[:1] + self.revisions[2:])

    def test_pull_branch(self):
        m = get_mercurial_vcs_obj()
        # Clone just the first rev
        m.clone(self.repodir, self.wc, revision=self.revisions[-1], update_dest=False)
        self.assertEquals(get_revisions(self.wc), self.revisions[-1:])

        # Now pull in the other branch
        rev = m.pull(self.repodir, self.wc, branch="branch2", update_dest=False)
        self.assertEquals(rev, None)

        # On hg 1.6, we'll be missing the last revision (on another branch)
        if m.hg_ver() >= (1, 6, 0):
            self.assertEquals(get_revisions(self.wc), self.revisions[1:])
        else:
            self.assertEquals(get_revisions(self.wc), self.revisions)

    def test_pull_unrelated(self):
        m = get_mercurial_vcs_obj()
        # Create a new repo
        repo2 = os.path.join(self.tmpdir, 'repo2')
        self._init_hg_repo(m, repo2)

        self.assertNotEqual(self.revisions, get_revisions(repo2))

        # Clone the original repo
        m.clone(self.repodir, self.wc, update_dest=False)
        # Hide the wanted error
        m.config = {'log_to_console': False}
        # Try and pull in changes from the new repo
        self.assertRaises(mercurial.VCSException, m.pull, repo2, self.wc, update_dest=False)

    def test_share_unrelated(self):
        m = get_mercurial_vcs_obj()
        # Create a new repo
        repo2 = os.path.join(self.tmpdir, 'repo2')
        self._init_hg_repo(m, repo2)

        self.assertNotEqual(self.revisions, get_revisions(repo2))

        share_base = os.path.join(self.tmpdir, 'share')

        # Clone the original repo
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'vcs_share_base': share_base}
        m.ensure_repo_and_revision()

        # Clone the new repo
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': repo2, 'dest': self.wc, 'vcs_share_base': share_base}
        m.ensure_repo_and_revision()

        self.assertEquals(get_revisions(self.wc), get_revisions(repo2))

    def test_share_reset(self):
        m = get_mercurial_vcs_obj()
        share_base = os.path.join(self.tmpdir, 'share')
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'vcs_share_base': share_base}

        # Clone the original repo
        m.ensure_repo_and_revision()

        old_revs = self.revisions[:]

        # Reset the repo
        self._init_hg_repo(m, self.repodir)

        self.assertNotEqual(old_revs, get_revisions(self.repodir))

        # Try and update our working copy
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'vcs_share_base': share_base}
        m.config = {'log_to_console': False}
        m.ensure_repo_and_revision()

        self.assertEquals(get_revisions(self.repodir), get_revisions(self.wc))
        self.assertNotEqual(old_revs, get_revisions(self.wc))

    def test_push(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[-2])
        m.push(src=self.repodir, remote=self.wc)
        self.assertEquals(get_revisions(self.wc), self.revisions)

    def test_push_with_branch(self):
        m = get_mercurial_vcs_obj()
        if m.hg_ver() >= (1, 6, 0):
            m.clone(self.repodir, self.wc, revision=self.revisions[-1])
            m.push(src=self.repodir, remote=self.wc, branch='branch2')
            m.push(src=self.repodir, remote=self.wc, branch='default')
            self.assertEquals(get_revisions(self.wc), self.revisions)

    def test_push_with_revision(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[-2])
        m.push(src=self.repodir, remote=self.wc, revision=self.revisions[-1])
        self.assertEquals(get_revisions(self.wc), self.revisions[-2:])

    def test_mercurial(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc}
        m.ensure_repo_and_revision()
        rev = m.ensure_repo_and_revision()
        self.assertEquals(rev, self.revisions[0])

    def test_push_new_branches_not_allowed(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc, revision=self.revisions[0])
        # Hide the wanted error
        m.config = {'log_to_console': False}
        self.assertRaises(Exception, m.push, self.repodir, self.wc, push_new_branches=False)

    def test_mercurial_with_new_share(self):
        m = get_mercurial_vcs_obj()
        share_base = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(share_base, self.repodir.lstrip("/"))
        os.mkdir(share_base)
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'vcs_share_base': share_base}
        m.ensure_repo_and_revision()
        self.assertEquals(get_revisions(self.repodir), get_revisions(self.wc))
        self.assertEquals(get_revisions(self.repodir), get_revisions(sharerepo))

    def test_mercurial_with_share_base_in_env(self):
        share_base = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(share_base, self.repodir.lstrip("/"))
        os.mkdir(share_base)
        try:
            os.environ['HG_SHARE_BASE_DIR'] = share_base
            m = get_mercurial_vcs_obj()
            m.vcs_config = {'repo': self.repodir, 'dest': self.wc}
            m.ensure_repo_and_revision()
            self.assertEquals(get_revisions(self.repodir), get_revisions(self.wc))
            self.assertEquals(get_revisions(self.repodir), get_revisions(sharerepo))
        finally:
            del os.environ['HG_SHARE_BASE_DIR']

    def test_mercurial_with_existing_share(self):
        m = get_mercurial_vcs_obj()
        share_base = os.path.join(self.tmpdir, 'share')
        sharerepo = os.path.join(share_base, self.repodir.lstrip("/"))
        os.mkdir(share_base)
        m.vcs_config = {'repo': self.repodir, 'dest': sharerepo}
        m.ensure_repo_and_revision()
        open(os.path.join(self.repodir, 'test.txt'), 'w').write('hello!')
        m.run_command(HG + ['add', 'test.txt'], cwd=self.repodir)
        m.run_command(HG + ['commit', '-m', 'adding changeset'], cwd=self.repodir)
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'vcs_share_base': share_base}
        m.ensure_repo_and_revision()
        self.assertEquals(get_revisions(self.repodir), get_revisions(self.wc))
        self.assertEquals(get_revisions(self.repodir), get_revisions(sharerepo))

    def test_mercurial_relative_dir(self):
        m = get_mercurial_vcs_obj()
        repo = os.path.basename(self.repodir)
        wc = os.path.basename(self.wc)
        m.vcs_config = {'repo': repo, 'dest': wc, 'revision': self.revisions[-1]}
        m.chdir(os.path.dirname(self.repodir))
        try:
            rev = m.ensure_repo_and_revision()
            self.assertEquals(rev, self.revisions[-1])
            m.info("Creating test.txt")
            open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

            m = get_mercurial_vcs_obj()
            m.vcs_config = {'repo': repo, 'dest': wc, 'revision': self.revisions[0]}
            rev = m.ensure_repo_and_revision()
            self.assertEquals(rev, self.revisions[0])
            # Make sure our local file didn't go away
            self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))
        finally:
            m.chdir(self.pwd)

    def test_mercurial_update_tip(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'revision': self.revisions[-1]}
        rev = m.ensure_repo_and_revision()
        self.assertEquals(rev, self.revisions[-1])
        open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc}
        rev = m.ensure_repo_and_revision()
        self.assertEquals(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))

    def test_mercurial_update_rev(self):
        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'revision': self.revisions[-1]}
        rev = m.ensure_repo_and_revision()
        self.assertEquals(rev, self.revisions[-1])
        open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

        m = get_mercurial_vcs_obj()
        m.vcs_config = {'repo': self.repodir, 'dest': self.wc, 'revision': self.revisions[0]}
        rev = m.ensure_repo_and_revision()
        self.assertEquals(rev, self.revisions[0])
        # Make sure our local file didn't go away
        self.failUnless(os.path.exists(os.path.join(self.wc, 'test.txt')))

    # TODO: this test doesn't seem to be compatible with mercurial()'s
    # share() usage, and fails when HG_SHARE_BASE_DIR is set
    def test_mercurial_change_repo(self):
        # Create a new repo
        old_env = os.environ.copy()
        if 'HG_SHARE_BASE_DIR' in os.environ:
            del os.environ['HG_SHARE_BASE_DIR']

        m = get_mercurial_vcs_obj()
        try:
            repo2 = os.path.join(self.tmpdir, 'repo2')
            self._init_hg_repo(m, repo2)

            self.assertNotEqual(self.revisions, get_revisions(repo2))

            # Clone the original repo
            m.vcs_config = {'repo': self.repodir, 'dest': self.wc}
            m.ensure_repo_and_revision()
            self.assertEquals(get_revisions(self.wc), self.revisions)
            open(os.path.join(self.wc, 'test.txt'), 'w').write("hello!")

            # Clone the new one
            m.vcs_config = {'repo': repo2, 'dest': self.wc}
            m.config = {'log_to_console': False}
            m.ensure_repo_and_revision()
            self.assertEquals(get_revisions(self.wc), get_revisions(repo2))
            # Make sure our local file went away
            self.failUnless(not os.path.exists(os.path.join(self.wc, 'test.txt')))
        finally:
            os.environ.clear()
            os.environ.update(old_env)

    def test_make_hg_url(self):
        #construct an hg url specific to revision, branch and filename and try to pull it down
        file_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            '//build/tools/',
            revision='FIREFOX_3_6_12_RELEASE',
            filename="/lib/python/util/hg.py",
            protocol='https',
        )
        expected_url = "https://hg.mozilla.org/build/tools/raw-file/FIREFOX_3_6_12_RELEASE/lib/python/util/hg.py"
        self.assertEquals(file_url, expected_url)

    def test_make_hg_url_no_filename(self):
        file_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            revision="default",
            protocol='https',
        )
        expected_url = "https://hg.mozilla.org/build/tools/rev/default"
        self.assertEquals(file_url, expected_url)

    def test_make_hg_url_no_revision_no_filename(self):
        repo_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            protocol='https',
        )
        expected_url = "https://hg.mozilla.org/build/tools"
        self.assertEquals(repo_url, expected_url)

    def test_make_hg_url_different_protocol(self):
        repo_url = mercurial.make_hg_url(
            "hg.mozilla.org",
            "/build/tools",
            protocol='ssh',
        )
        expected_url = "ssh://hg.mozilla.org/build/tools"
        self.assertEquals(repo_url, expected_url)

    def test_share_repo(self):
        m = get_mercurial_vcs_obj()
        repo3 = os.path.join(self.tmpdir, 'repo3')
        m.share(self.repodir, repo3)
        # make sure shared history is identical
        self.assertEquals(self.revisions, get_revisions(repo3))

    def test_mercurial_share_outgoing(self):
        m = get_mercurial_vcs_obj()
        # ensure that outgoing changesets in a shared clone affect the shared history
        repo5 = os.path.join(self.tmpdir, 'repo5')
        repo6 = os.path.join(self.tmpdir, 'repo6')
        m.vcs_config = {'repo': self.repodir, 'dest': repo5}
        m.ensure_repo_and_revision()
        m.share(repo5, repo6)
        open(os.path.join(repo6, 'test.txt'), 'w').write("hello!")
        # modify the history of the new clone
        m.run_command(HG + ['add', 'test.txt'], cwd=repo6)
        m.run_command(HG + ['commit', '-m', 'adding changeset'], cwd=repo6)
        self.assertNotEquals(self.revisions, get_revisions(repo6))
        self.assertNotEquals(self.revisions, get_revisions(repo5))
        self.assertEquals(get_revisions(repo5), get_revisions(repo6))

    def test_apply_and_push(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)

        def c(repo, attempt):
            m.run_command(HG + ['tag', '-f', 'TEST'], cwd=repo)
        m.apply_and_push(self.wc, self.repodir, c)
        self.assertEquals(get_revisions(self.wc), get_revisions(self.repodir))

    def test_apply_and_push_fail(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)

        def c(repo, attempt, remote):
            m.run_command(HG + ['tag', '-f', 'TEST'], cwd=repo)
            m.run_command(HG + ['tag', '-f', 'CONFLICTING_TAG'], cwd=remote)
        m.config = {'log_to_console': False}
        self.assertRaises(errors.VCSException, m.apply_and_push, self.wc,
                          self.repodir, lambda r, a: c(r, a, self.repodir),
                          max_attempts=2)

    def test_apply_and_push_with_rebase(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        m.config = {'log_to_console': False}

        def c(repo, attempt, remote):
            m.run_command(HG + ['tag', '-f', 'TEST'], cwd=repo)
            if attempt == 1:
                m.run_command(HG + ['rm', 'hello.txt'], cwd=remote)
                m.run_command(HG + ['commit', '-m', 'test'], cwd=remote)
        m.apply_and_push(self.wc, self.repodir,
                         lambda r, a: c(r, a, self.repodir), max_attempts=2)
        self.assertEquals(get_revisions(self.wc), get_revisions(self.repodir))

    def test_apply_and_push_rebase_fails(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)
        m.config = {'log_to_console': False}

        def c(repo, attempt, remote):
            m.run_command(HG + ['tag', '-f', 'TEST'], cwd=repo)
            if attempt in (1, 2):
                m.run_command(HG + ['tag', '-f', 'CONFLICTING_TAG'], cwd=remote)
        m.apply_and_push(self.wc, self.repodir,
                         lambda r, a: c(r, a, self.repodir), max_attempts=4)
        self.assertEquals(get_revisions(self.wc), get_revisions(self.repodir))

    def test_apply_and_push_on_branch(self):
        m = get_mercurial_vcs_obj()
        if m.hg_ver() >= (1, 6, 0):
            m.clone(self.repodir, self.wc)

            def c(repo, attempt):
                m.run_command(HG + ['branch', 'branch3'], cwd=repo)
                m.run_command(HG + ['tag', '-f', 'TEST'], cwd=repo)
            m.apply_and_push(self.wc, self.repodir, c)
            self.assertEquals(get_revisions(self.wc), get_revisions(self.repodir))

    def test_apply_and_push_with_no_change(self):
        m = get_mercurial_vcs_obj()
        m.clone(self.repodir, self.wc)

        def c(r, a):
            pass
        self.assertRaises(errors.VCSException, m.apply_and_push, self.wc, self.repodir, c)

if __name__ == '__main__':
    unittest.main()
