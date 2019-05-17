# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import os

import mozunit

from unittest import expectedFailure

from mozpack.copier import (
    FileCopier,
    FileRegistry,
)
from mozpack.manifests import (
    InstallManifest,
    UnreadableInstallManifest,
)
from mozpack.test.test_files import TestWithTmpDir

import mozbuild.action.process_install_manifest as process_install_manifest


class TestGenerateManifest(TestWithTmpDir):
    """
    Unit tests for process_install_manifest.py.
    """

    def test_process_manifest(self):
        source = self.tmppath('source')
        os.mkdir(source)
        os.mkdir('%s/base' % source)
        os.mkdir('%s/base/foo' % source)
        os.mkdir('%s/base2' % source)

        with open('%s/base/foo/file1' % source, 'a'):
            pass

        with open('%s/base/foo/file2' % source, 'a'):
            pass

        with open('%s/base2/file3' % source, 'a'):
            pass

        m = InstallManifest()
        m.add_pattern_link('%s/base' % source, '**', '')
        m.add_link('%s/base2/file3' % source, 'foo/file3')

        p = self.tmppath('m')
        m.write(path=p)

        dest = self.tmppath('dest')
        track = self.tmppath('track')

        for i in range(2):
            process_install_manifest.process_manifest(dest, [p], track)

            self.assertTrue(os.path.exists(self.tmppath('dest/foo/file1')))
            self.assertTrue(os.path.exists(self.tmppath('dest/foo/file2')))
            self.assertTrue(os.path.exists(self.tmppath('dest/foo/file3')))

        m = InstallManifest()
        m.write(path=p)

        for i in range(2):
            process_install_manifest.process_manifest(dest, [p], track)

            self.assertFalse(os.path.exists(self.tmppath('dest/foo/file1')))
            self.assertFalse(os.path.exists(self.tmppath('dest/foo/file2')))
            self.assertFalse(os.path.exists(self.tmppath('dest/foo/file3')))

if __name__ == '__main__':
    mozunit.main()
