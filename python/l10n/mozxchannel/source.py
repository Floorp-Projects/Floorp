# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import toml
from compare_locales import mozpath, paths
from compare_locales.paths.matcher import expand

from .projectconfig import generate_filename


class HGFiles(paths.ProjectFiles):
    def __init__(self, repo, rev, projects):
        self.repo = repo
        self.ctx = repo[rev]
        self.root = repo.root()
        self.manifest = None
        self.configs_map = {}
        # get paths for our TOML files
        for p in projects:
            all_configpaths = {
                mozpath.abspath(c.path).encode("utf-8") for c in p.configs
            }
            for refpath in all_configpaths:
                local_path = mozpath.relpath(refpath, self.root)
                if local_path not in self.ctx:
                    print("ignoring", refpath)
                    continue
                targetpath = b"/".join(
                    (
                        expand(None, "{l10n_base}", p.environ).encode("utf-8"),
                        b"en-US",
                        generate_filename(local_path),
                    )
                )
                self.configs_map[refpath] = targetpath
        super(HGFiles, self).__init__("en-US", projects)
        for m in self.matchers:
            m["l10n"].encoding = "utf-8"
            if "reference" in m:
                m["reference"].encoding = "utf-8"
        if self.exclude:
            for m in self.exclude.matchers:
                m["l10n"].encoding = "utf-8"
                if "reference" in m:
                    m["reference"].encoding = "utf-8"

    def _files(self, matcher):
        for f in self.ctx.manifest():
            f = mozpath.join(self.root, f)
            if matcher.match(f):
                yield f

    def __iter__(self):
        for t in super(HGFiles, self).__iter__():
            yield t
        for refpath, targetpath in self.configs_map.items():
            yield targetpath, refpath, None, set()

    def match(self, path):
        m = super(HGFiles, self).match(path)
        if m:
            return m
        for refpath, targetpath in self.configs_map.items():
            if path in [refpath, targetpath]:
                return targetpath, refpath, None, set()


class HgTOMLParser(paths.TOMLParser):
    "subclass to load from our hg context"

    def __init__(self, repo, rev):
        self.repo = repo
        self.rev = rev
        self.root = repo.root().decode("utf-8")

    def load(self, parse_ctx):
        try:
            path = parse_ctx.path
            local_path = "path:" + mozpath.relpath(path, self.root)
            data = self.repo.cat(files=[local_path.encode("utf-8")], rev=self.rev)
        except Exception:
            raise paths.ConfigNotFound(parse_ctx.path)

        try:
            parse_ctx.data = toml.loads(data.decode())
        except toml.TomlDecodeError as e:
            raise RuntimeError(f"In file '{parse_ctx.path}':\n  {e!s}") from e
