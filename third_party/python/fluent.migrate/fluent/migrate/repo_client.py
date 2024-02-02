from __future__ import annotations
from typing import Tuple

import json
from subprocess import run

from os.path import isdir, join

import hglib


def git(root: str, *args: str) -> str:
    """
    Wrapper for calling command-line git in the `root` directory.
    Raises an exception on any error, including a non-0 return code.
    Returns the command's stdout as a string.
    """
    git = ["git"]
    git.extend(args)
    proc = run(git, capture_output=True, cwd=root, encoding="utf-8")
    if proc.returncode != 0:
        raise Exception(proc.stderr or f"git command failed: {args}")
    return proc.stdout


class RepoClient:
    def __init__(self, root: str):
        self.root = root
        if isdir(join(root, ".hg")):
            self.hgclient = hglib.open(root, "utf-8")
        elif isdir(join(root, ".git")):
            self.hgclient = None
            stdout = git(self.root, "rev-parse", "--is-inside-work-tree")
            if stdout != "true\n":
                raise Exception("git rev-parse failed")
        else:
            raise Exception(f"Unsupported repository: {root}")

    def close(self):
        if self.hgclient:
            self.hgclient.close()

    def blame(self, file: str) -> list[Tuple[str, int]]:
        "Return a list of (author, time) tuples for each line in `file`."
        if self.hgclient:
            args = hglib.util.cmdbuilder(
                b"annotate",
                file.encode("latin-1"),
                template="json",
                date=True,
                user=True,
                cwd=self.root,
            )
            blame_json = self.hgclient.rawcommand(args)
            return [
                (line["user"], int(line["date"][0]))
                for line in json.loads(blame_json)[0]["lines"]
            ]
        else:
            lines: list[Tuple[str, int]] = []
            user = ""
            time = 0
            stdout = git(self.root, "blame", "--porcelain", file)
            for line in stdout.splitlines():
                if line.startswith("author "):
                    user = line[7:]
                elif line.startswith("author-mail "):
                    user += line[11:]  # includes leading space
                elif line.startswith("author-time "):
                    time = int(line[12:])
                elif line.startswith("\t"):
                    lines.append((user, time))
            return lines

    def commit(self, message: str, author: str):
        "Add and commit all work tree files"
        if self.hgclient:
            self.hgclient.commit(message, user=author.encode("utf-8"), addremove=True)
        else:
            git(self.root, "add", ".")
            git(self.root, "commit", f"--author={author}", f"--message={message}")

    def head(self) -> str:
        "Identifier for the most recent commit"
        if self.hgclient:
            return self.hgclient.tip().node.decode("utf-8")
        else:
            return git(self.root, "rev-parse", "HEAD").strip()

    def log(self, from_commit: str, to_commit: str) -> list[str]:
        if self.hgclient:
            return [
                rev.desc.decode("utf-8")
                for rev in self.hgclient.log(f"{to_commit} % {from_commit}")
            ]
        else:
            return (
                git(
                    self.root,
                    "log",
                    "--pretty=format:%s",
                    f"{from_commit}..{to_commit}",
                )
                .strip()
                .splitlines()
            )
