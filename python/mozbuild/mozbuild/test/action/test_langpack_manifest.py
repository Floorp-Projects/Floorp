# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import json
import os
import shutil
import tempfile
import unittest

import mozunit

from mozbuild.action import langpack_manifest


class TestGenerateManifest(unittest.TestCase):
    """
    Unit tests for langpack_manifest.py.
    """

    def test_parse_flat_ftl(self):
        src = """
langpack-creator = bar {"bar"}
langpack-contributors = { "" }
"""
        tmp = tempfile.NamedTemporaryFile(mode="wt", suffix=".ftl", delete=False)
        try:
            tmp.write(src)
            tmp.close()
            ftl = langpack_manifest.parse_flat_ftl(tmp.name)
            self.assertEqual(ftl["langpack-creator"], "bar bar")
            self.assertEqual(ftl["langpack-contributors"], "")
        finally:
            os.remove(tmp.name)

    def test_parse_flat_ftl_missing(self):
        ftl = langpack_manifest.parse_flat_ftl("./does-not-exist.ftl")
        self.assertEqual(len(ftl), 0)

    def test_manifest(self):
        ctx = {
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
        self.assertEqual(data["name"], "Language: Suomi (Finnish)")
        self.assertEqual(
            data["description"], "Firefox Language Pack for Suomi (fi) – Finnish"
        )
        self.assertEqual(
            data["author"], "Suomennosprojekti (contributors: Joe Smith, Mary White)"
        )
        self.assertEqual(data["version"], "57.0.20210928.100000")

    def test_manifest_truncated_name(self):
        ctx = {
            "langpack-creator": "Mozilla.org / Softcatalà",
            "langpack-contributors": "Joe Smith, Mary White",
        }
        os.environ["MOZ_BUILD_DATE"] = "20210928100000"
        manifest = langpack_manifest.create_webmanifest(
            "ca-valencia",
            "57.0.1",
            "57.0",
            "57.0.*",
            "Firefox",
            "/var/vcs/l10n-central",
            "langpack-ca-valencia@firefox.mozilla.og",
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEqual(data["name"], "Language: Català (Valencià)")
        self.assertEqual(
            data["description"],
            "Firefox Language Pack for Català (Valencià) (ca-valencia) – Catalan, Valencian",
        )

    def test_manifest_name_untranslated(self):
        ctx = {
            "langpack-creator": "Mozilla.org",
            "langpack-contributors": "Joe Smith, Mary White",
        }
        os.environ["MOZ_BUILD_DATE"] = "20210928100000"
        manifest = langpack_manifest.create_webmanifest(
            "en-US",
            "57.0.1",
            "57.0",
            "57.0.*",
            "Firefox",
            "/var/vcs/l10n-central",
            "langpack-ca-valencia@firefox.mozilla.og",
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEqual(data["name"], "Language: English (US)")
        self.assertEqual(
            data["description"],
            "Firefox Language Pack for English (US) (en-US)",
        )

    def test_manifest_without_contributors(self):
        ctx = {
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
        self.assertEqual(data["name"], "Language: Suomi (Finnish)")
        self.assertEqual(
            data["description"], "Firefox Language Pack for Suomi (fi) – Finnish"
        )
        self.assertEqual(data["author"], "Suomennosprojekti")

    def test_manifest_truncation(self):
        locale = (
            "Long locale code that will be truncated and will cause both "
            "the name and the description to exceed the maximum number of "
            "characters allowed in manifest.json"
        )
        title, description = langpack_manifest.get_title_and_description(
            "Firefox", locale
        )

        self.assertEqual(len(title), 45)
        self.assertEqual(len(description), 132)

    def test_get_version_maybe_buildid(self):
        for app_version, buildid, expected_version in [
            ("109", "", "109"),
            ("109.0", "", "109.0"),
            ("109.0.0", "", "109.0.0"),
            ("109", "20210928", "109"),  # buildid should be 14 chars
            ("109", "20210928123456", "109.20210928.123456"),
            ("109.0", "20210928123456", "109.0.20210928.123456"),
            ("109.0.0", "20210928123456", "109.0.20210928.123456"),
            ("109", "20230215023456", "109.20230215.23456"),
            ("109.0", "20230215023456", "109.0.20230215.23456"),
            ("109.0.0", "20230215023456", "109.0.20230215.23456"),
            ("109", "20230215003456", "109.20230215.3456"),
            ("109", "20230215000456", "109.20230215.456"),
            ("109", "20230215000056", "109.20230215.56"),
            ("109", "20230215000006", "109.20230215.6"),
            ("109", "20230215000000", "109.20230215.0"),
            ("109.1.2.3", "20230201000000", "109.1.20230201.0"),
            ("109.0a1", "", "109.0"),
            ("109a0.0b0", "", "109.0"),
            ("109.0.0b1", "", "109.0.0"),
            ("109.0.b1", "", "109.0.0"),
            ("109..1", "", "109.0.1"),
        ]:
            os.environ["MOZ_BUILD_DATE"] = buildid
            version = langpack_manifest.get_version_maybe_buildid(app_version)
            self.assertEqual(version, expected_version)

    def test_main(self):
        # We set this env variable so that the manifest.json version string
        # uses a "buildid", see: `get_version_maybe_buildid()` for more
        # information.
        os.environ["MOZ_BUILD_DATE"] = "20210928100000"

        TEST_CASES = [
            {
                "app_version": "112.0.1",
                "max_app_version": "112.*",
                "expected_version": "112.0.20210928.100000",
                "expected_min_version": "112.0",
                "expected_max_version": "112.*",
            },
            {
                "app_version": "112.1.0",
                "max_app_version": "112.*",
                "expected_version": "112.1.20210928.100000",
                # We expect the second part to be "0" even if the app version
                # has a minor part equal to "1".
                "expected_min_version": "112.0",
                "expected_max_version": "112.*",
            },
            {
                "app_version": "114.0a1",
                "max_app_version": "114.*",
                "expected_version": "114.0.20210928.100000",
                # We expect the min version to be equal to the app version
                # because we don't change alpha versions.
                "expected_min_version": "114.0a1",
                "expected_max_version": "114.*",
            },
        ]

        tmpdir = tempfile.mkdtemp()
        try:
            # These files are required by the `main()` function.
            for file in ["chrome.manifest", "empty-metadata.ftl"]:
                langpack_manifest.write_file(os.path.join(tmpdir, file), "")

            for tc in TEST_CASES:
                extension_id = "some@extension-id"
                locale = "fr"

                args = [
                    "--input",
                    tmpdir,
                    # This file has been created right above.
                    "--metadata",
                    "empty-metadata.ftl",
                    "--app-name",
                    "Firefox",
                    "--l10n-basedir",
                    "/var/vcs/l10n-central",
                    "--locales",
                    locale,
                    "--langpack-eid",
                    extension_id,
                    "--app-version",
                    tc["app_version"],
                    "--max-app-ver",
                    tc["max_app_version"],
                ]
                langpack_manifest.main(args)

                with open(os.path.join(tmpdir, "manifest.json")) as manifest_file:
                    manifest = json.load(manifest_file)
                    self.assertEqual(manifest["version"], tc["expected_version"])
                    self.assertEqual(manifest["langpack_id"], locale)
                    self.assertEqual(
                        manifest["browser_specific_settings"],
                        {
                            "gecko": {
                                "id": extension_id,
                                "strict_min_version": tc["expected_min_version"],
                                "strict_max_version": tc["expected_max_version"],
                            }
                        },
                    )
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)
            del os.environ["MOZ_BUILD_DATE"]


if __name__ == "__main__":
    mozunit.main()
