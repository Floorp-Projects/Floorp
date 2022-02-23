# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import, print_function

import unittest
import json
import os

import mozunit

import mozbuild.action.langpack_manifest as langpack_manifest
from mozbuild.preprocessor import Context


class TestGenerateManifest(unittest.TestCase):
    """
    Unit tests for langpack_manifest.py.
    """

    def test_manifest(self):
        ctx = Context()
        ctx["MOZ_LANG_TITLE"] = "Finnish"
        ctx["MOZ_LANGPACK_CREATOR"] = "Suomennosprojekti"
        ctx[
            "MOZ_LANGPACK_CONTRIBUTORS"
        ] = """
            <em:contributor>Joe Smith</em:contributor>
            <em:contributor>Mary White</em:contributor>
        """
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
        ctx = Context()
        ctx["MOZ_LANG_TITLE"] = "Finnish"
        ctx["MOZ_LANGPACK_CREATOR"] = "Suomennosprojekti"
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
