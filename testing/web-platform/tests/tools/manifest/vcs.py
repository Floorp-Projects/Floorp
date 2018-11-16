import json
import os
import platform
import subprocess

from .sourcefile import SourceFile


class Git(object):
    def __init__(self, repo_root, url_base, filters=None):
        self.root = os.path.abspath(repo_root)
        self.git = Git.get_func(repo_root)
        self.url_base = url_base

    @staticmethod
    def get_func(repo_path):
        def git(cmd, *args):
            full_cmd = ["git", cmd] + list(args)
            try:
                return subprocess.check_output(full_cmd, cwd=repo_path, stderr=subprocess.STDOUT)
            except Exception as e:
                if platform.uname()[0] == "Windows" and isinstance(e, WindowsError):
                        full_cmd[0] = "git.bat"
                        return subprocess.check_output(full_cmd, cwd=repo_path, stderr=subprocess.STDOUT)
                else:
                    raise
        return git

    @classmethod
    def for_path(cls, path, url_base):
        git = Git.get_func(path)
        try:
            return cls(git("rev-parse", "--show-toplevel").rstrip(), url_base)
        except subprocess.CalledProcessError:
            return None

    def _local_changes(self):
        changes = {}
        cmd = ["status", "-z", "--ignore-submodules=all"]
        data = self.git(*cmd)

        if data == "":
            return changes

        rename_data = None
        for entry in data.split("\0")[:-1]:
            if rename_data is not None:
                status, rel_path = entry.split(" ")
                if status[0] == "R":
                    rename_data = (rel_path, status)
                else:
                    changes[rel_path] = (status, None)
            else:
                rel_path = entry
                changes[rel_path] = rename_data
                rename_data = None
        return changes

    def _show_file(self, path):
        path = os.path.relpath(os.path.abspath(path), self.root)
        return self.git("show", "HEAD:%s" % path)

    def __iter__(self):
        cmd = ["ls-tree", "-r", "-z", "HEAD"]
        local_changes = self._local_changes()
        for result in self.git(*cmd).split("\0")[:-1]:
            rel_path = result.split("\t")[-1]
            hash = result.split()[2]
            if not os.path.isdir(os.path.join(self.root, rel_path)):
                if rel_path in local_changes:
                    contents = self._show_file(rel_path)
                else:
                    contents = None
                yield SourceFile(self.root,
                                 rel_path,
                                 self.url_base,
                                 hash,
                                 contents=contents), True


class FileSystem(object):
    def __init__(self, root, url_base, mtime_filter):
        self.root = root
        self.url_base = url_base
        from gitignore import gitignore
        self.path_filter = gitignore.PathFilter(self.root, extras=[".git/"])
        self.mtime_filter = mtime_filter

    def __iter__(self):
        mtime_cache = self.mtime_cache
        for dirpath, dirnames, filenames in self.path_filter(walk(".")):
            for filename, path_stat in filenames:
                # We strip the ./ prefix off the path
                path = os.path.join(dirpath, filename)
                if mtime_cache is None or mtime_cache.updated(path, path_stat):
                    yield SourceFile(self.root, path, self.url_base), True
                else:
                    yield path, False
        self.ignore_cache.dump()

    def dump_caches(self):
        for cache in [self.mtime_cache, self.ignore_cache]:
            if cache is not None:
                cache.dump()


class CacheFile(object):
    file_name = None

    def __init__(self, cache_root, rebuild=False):
        if not os.path.exists(cache_root):
            os.makedirs(cache_root)
        self.path = os.path.join(cache_root, self.file_name)
        self.data = self.load(rebuild)
        self.modified = False

    def dump(self):
        missing = set(self.data.keys()) - self.updated
        if not missing or not self.modified:
            return
        for item in missing:
            del self.data[item]
        with open(self.path, 'w') as f:
            json.dump(self.data, f, indent=1)

    def load(self):
        try:
            with open(self.path, 'r') as f:
                return json.load(f)
        except IOError:
            return {}

    def update(self, rel_path, stat=None):
        self.updated.add(rel_path)
        try:
            if stat is None:
                stat = os.stat(os.path.join(self.root,
                                            rel_path))
        except Exception:
            return True

        mtime = stat.st_mtime
        if mtime != self.data.get(rel_path):
            self.modified = True
            self.data[rel_path] = mtime
            return True
        return False
