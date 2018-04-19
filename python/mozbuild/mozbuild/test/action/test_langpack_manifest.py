# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import unittest
import json

import mozunit

import mozbuild.action.langpack_manifest as langpack_manifest
from mozbuild.preprocessor import Context


class TestGenerateManifest(unittest.TestCase):
    """
    Unit tests for langpack_manifest.py.
    """

    def test_manifest(self):
        ctx = Context()
        ctx['MOZ_LANG_TITLE'] = 'Finnish'
        ctx['MOZ_LANGPACK_CREATOR'] = 'Suomennosprojekti'
        ctx['MOZ_LANGPACK_CONTRIBUTORS'] = """
            <em:contributor>Joe Smith</em:contributor>
            <em:contributor>Mary White</em:contributor>
        """
        manifest = langpack_manifest.create_webmanifest(
            'fi',
            '57.0',
            '57.0.*',
            'Firefox',
            '/var/vcs/l10n-central',
            'langpack-fi@firefox.mozilla.og',
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEquals(data['name'], 'Finnish Language Pack')
        self.assertEquals(
            data['author'], 'Suomennosprojekti (contributors: Joe Smith, Mary White)')

    def test_manifest_without_contributors(self):
        ctx = Context()
        ctx['MOZ_LANG_TITLE'] = 'Finnish'
        ctx['MOZ_LANGPACK_CREATOR'] = 'Suomennosprojekti'
        manifest = langpack_manifest.create_webmanifest(
            'fi',
            '57.0',
            '57.0.*',
            'Firefox',
            '/var/vcs/l10n-central',
            'langpack-fi@firefox.mozilla.og',
            ctx,
            {},
        )

        data = json.loads(manifest)
        self.assertEquals(data['name'], 'Finnish Language Pack')
        self.assertEquals(data['author'], 'Suomennosprojekti')


if __name__ == '__main__':
    mozunit.main()
