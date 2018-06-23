import os
import subprocess

class Mercurial(object):
    def __init__(self, repo_root):
        self.root = os.path.abspath(repo_root)
        self.hg = Mercurial.get_func(repo_root)

    @staticmethod
    def get_func(repo_root):
        def hg(cmd, *args):
            full_cmd = ["hg", cmd] + list(args)
            return subprocess.check_output(full_cmd, cwd=repo_root)
            # TODO: Test on Windows.
        return hg

    @staticmethod
    def is_hg_repo(repo_root):
        try:
            with open(os.devnull, 'w') as devnull:
                subprocess.check_call(["hg", "root"], cwd=repo_root, stdout=devnull,
                                        stderr=devnull)
        except subprocess.CalledProcessError:
            return False
        # TODO: Test on windows
        return True


class Git(object):
    def __init__(self, repo_root, url_base):
        self.root = os.path.abspath(repo_root)
        self.git = Git.get_func(repo_root)

    @staticmethod
    def get_func(repo_root):
        def git(cmd, *args):
            full_cmd = ["git", cmd] + list(args)
            return subprocess.check_output(full_cmd, cwd=repo_root)
            # TODO: Test on Windows.
        return git

    @staticmethod
    def is_git_repo(repo_root):
        try:
            with open(os.devnull, 'w') as devnull:
                subprocess.check_call(["git", "rev-parse", "--show-cdup"], cwd=repo_root,
                                        stdout=devnull, stderr=devnull)
        except subprocess.CalledProcessError:
            return False
        # TODO: Test on windows
        return True
