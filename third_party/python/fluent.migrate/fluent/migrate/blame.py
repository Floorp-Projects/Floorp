from __future__ import annotations
from typing import Dict, Iterable, Tuple, TypedDict, cast

import argparse
import json
from os.path import join

from compare_locales.parser import Junk, getParser
from compare_locales.parser.fluent import FluentEntity

from .repo_client import RepoClient

BlameData = Dict[str, Dict[str, Tuple[int, float]]]
"File path -> message key -> [userid, timestamp]"


class BlameResult(TypedDict):
    authors: list[str]
    blame: BlameData


class Blame:
    def __init__(self, client: RepoClient):
        self.client = client
        self.users: list[str] = []
        self.blame: BlameData = {}

    def attribution(self, file_paths: Iterable[str]) -> BlameResult:
        for file in file_paths:
            blame = self.client.blame(file)
            self.handleFile(file, blame)
        return {"authors": self.users, "blame": self.blame}

    def handleFile(self, path: str, file_blame: list[Tuple[str, int]]):
        try:
            parser = getParser(path)
        except UserWarning:
            return

        self.blame[path] = {}

        self.readFile(parser, path)
        entities = parser.parse()
        for e in entities:
            if isinstance(e, Junk):
                continue
            if e.val_span:
                key_vals: list[tuple[str, str]] = [(e.key, e.val_span)]
            else:
                key_vals = []
            if isinstance(e, FluentEntity):
                key_vals += [
                    (f"{e.key}.{attr.key}", cast(str, attr.val_span))
                    for attr in e.attributes
                ]
            for key, (val_start, val_end) in key_vals:
                entity_lines = file_blame[
                    (e.ctx.linecol(val_start)[0] - 1) : e.ctx.linecol(val_end)[0]
                ]
                user, timestamp = max(entity_lines, key=lambda x: x[1])
                if user not in self.users:
                    self.users.append(user)
                userid = self.users.index(user)
                self.blame[path][key] = (userid, timestamp)

    def readFile(self, parser, path: str):
        parser.readFile(join(self.client.root, path))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo_path")
    parser.add_argument("file_path", nargs="+")
    args = parser.parse_args()
    blame = Blame(RepoClient(args.repo_path))
    attrib = blame.attribution(args.file_path)
    print(json.dumps(attrib, indent=4, separators=(",", ": ")))
