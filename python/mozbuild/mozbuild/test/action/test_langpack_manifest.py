# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function

import json
import os
import tempfile
import unittest

import mozbuild.action.langpack_manifest as langpack_manifest
import mozunit


class TestGenerateManifest(unittest.TestCase):
    """
    Unit tests for langpack_manifest.py.
    """

    def test_parse_flat_ftl(self):
        src = """
langpack-title = foo
langpack-creator = bar {"bar"}
langpack-contributors = { "" }
"""
        tmp = tempfile.NamedTemporaryFile(mode="wt", suffix=".ftl", delete=False)
        try:
            tmp.write(src)
            tmp.close()
            ftl = langpack_manifest.parse_flat_ftl(tmp.name)
            self.assertEqual(ftl["langpack-title"], "foo")
            self.assertEqual(ftl["langpack-creator"], "bar bar")
            self.assertEqual(ftl["langpack-contributors"], "")
        finally:
            os.remove(tmp.name)

    def test_parse_flat_ftl_missing(self):
        ftl = langpack_manifest.parse_flat_ftl("./does-not-exist.ftl")
        self.assertEqual(len(ftl), 0)

    def test_manifest(self):
        ctx = {
            "langpack-title": "Finnish",
            "langpack-creator": "Suomennosprojekti",
            "langpack-contributors": "Joe Smith, Mary White",
        }
        os.environ["MOZ_BUILD_DATE"] = "20210928100000"
        manifest = langpack_manifest.create_webmanifest(
            "fi",
            "57.0.1",
            "57.0",
            "57.0.*",
            "Firefox",
            "/var/vcs/l10n-central",
            "langpack-fi@firefox.mozilla.og",
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEqual(data["name"], "Finnish Language Pack")
        self.assertEqual(
            data["author"], "Suomennosprojekti (contributors: Joe Smith, Mary White)"
        )
        self.assertEqual(data["version"], "57.0.1buildid20210928.100000")

    def test_manifest_without_contributors(self):
        ctx = {
            "langpack-title": "Finnish",
            "langpack-creator": "Suomennosprojekti",
            "langpack-contributors": "",
        }
        manifest = langpack_manifest.create_webmanifest(
            "fi",
            "57.0.1",
            "57.0",
            "57.0.*",
            "Firefox",
            "/var/vcs/l10n-central",
            "langpack-fi@firefox.mozilla.og",
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEqual(data["name"], "Finnish Language Pack")
        self.assertEqual(data["author"], "Suomennosprojekti")


if __name__ == "__main__":
    mozunit.main()
