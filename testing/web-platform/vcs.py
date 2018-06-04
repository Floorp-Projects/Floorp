import os
import subprocess

class Mercurial(object):

    def __init__(self, repo_root):
        self.root = os.path.abspath(repo_root)
        self.hg = Mercurial.get_func(repo_root)


    @staticmethod
    def get_func(repo_path):
        def hg(cmd, *args):
            full_cmd = ["hg", cmd] + list(args)
            try:
                return subprocess.check_output(full_cmd, cwd=repo_path, stderr=subprocess.STDOUT)
            except Exception as e:
                raise(e)
            # TODO: Test on Windows.
        return hg
