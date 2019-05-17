#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import tempfile
import unittest

from StringIO import StringIO

import mozunit

from mozbuild.backend.configenvironment import ConfigEnvironment

from mozbuild.mozinfo import (
    build_dict,
    write_mozinfo,
)

from mozfile.mozfile import NamedTemporaryFile


class Base(object):
    def _config(self, substs={}):
        d = os.path.dirname(__file__)
        return ConfigEnvironment(d, d, substs=substs)


class TestBuildDict(unittest.TestCase, Base):
    def test_missing(self):
        """
        Test that missing required values raises.
        """

        with self.assertRaises(Exception):
            build_dict(self._config(substs=dict(OS_TARGET='foo')))

        with self.assertRaises(Exception):
            build_dict(self._config(substs=dict(TARGET_CPU='foo')))

        with self.assertRaises(Exception):
            build_dict(self._config(substs=dict(MOZ_WIDGET_TOOLKIT='foo')))

    def test_win(self):
        d = build_dict(self._config(dict(
            OS_TARGET='WINNT',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='windows',
        )))
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('windows', d['toolkit'])
        self.assertEqual(32, d['bits'])

    def test_linux(self):
        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('gtk3', d['toolkit'])
        self.assertEqual(32, d['bits'])

        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='x86_64',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual('gtk3', d['toolkit'])
        self.assertEqual(64, d['bits'])

    def test_mac(self):
        d = build_dict(self._config(dict(
            OS_TARGET='Darwin',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='cocoa',
        )))
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertEqual(32, d['bits'])

        d = build_dict(self._config(dict(
            OS_TARGET='Darwin',
            TARGET_CPU='x86_64',
            MOZ_WIDGET_TOOLKIT='cocoa',
        )))
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertEqual(64, d['bits'])

    def test_android(self):
        d = build_dict(self._config(dict(
            OS_TARGET='Android',
            TARGET_CPU='arm',
            MOZ_WIDGET_TOOLKIT='android',
        )))
        self.assertEqual('android', d['os'])
        self.assertEqual('arm', d['processor'])
        self.assertEqual('android', d['toolkit'])
        self.assertEqual(32, d['bits'])

    def test_x86(self):
        """
        Test that various i?86 values => x86.
        """
        d = build_dict(self._config(dict(
            OS_TARGET='WINNT',
            TARGET_CPU='i486',
            MOZ_WIDGET_TOOLKIT='windows',
        )))
        self.assertEqual('x86', d['processor'])

        d = build_dict(self._config(dict(
            OS_TARGET='WINNT',
            TARGET_CPU='i686',
            MOZ_WIDGET_TOOLKIT='windows',
        )))
        self.assertEqual('x86', d['processor'])

    def test_arm(self):
        """
        Test that all arm CPU architectures => arm.
        """
        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='arm',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual('arm', d['processor'])

        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='armv7',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual('arm', d['processor'])

    def test_unknown(self):
        """
        Test that unknown values pass through okay.
        """
        d = build_dict(self._config(dict(
            OS_TARGET='RandOS',
            TARGET_CPU='cptwo',
            MOZ_WIDGET_TOOLKIT='foobar',
        )))
        self.assertEqual("randos", d["os"])
        self.assertEqual("cptwo", d["processor"])
        self.assertEqual("foobar", d["toolkit"])
        # unknown CPUs should not get a bits value
        self.assertFalse("bits" in d)

    def test_debug(self):
        """
        Test that debug values are properly detected.
        """
        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual(False, d['debug'])

        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='gtk3',
            MOZ_DEBUG='1',
        )))
        self.assertEqual(True, d['debug'])

    def test_crashreporter(self):
        """
        Test that crashreporter values are properly detected.
        """
        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='gtk3',
        )))
        self.assertEqual(False, d['crashreporter'])

        d = build_dict(self._config(dict(
            OS_TARGET='Linux',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='gtk3',
            MOZ_CRASHREPORTER='1',
        )))
        self.assertEqual(True, d['crashreporter'])


class TestWriteMozinfo(unittest.TestCase, Base):
    """
    Test the write_mozinfo function.
    """
    def setUp(self):
        fd, self.f = tempfile.mkstemp()
        os.close(fd)

    def tearDown(self):
        os.unlink(self.f)

    def test_basic(self):
        """
        Test that writing to a file produces correct output.
        """
        c = self._config(dict(
            OS_TARGET='WINNT',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='windows',
        ))
        tempdir = tempfile.tempdir
        c.topsrcdir = tempdir
        with NamedTemporaryFile(dir=os.path.normpath(c.topsrcdir)) as mozconfig:
            mozconfig.write('unused contents')
            mozconfig.flush()
            c.mozconfig = mozconfig.name
            write_mozinfo(self.f, c)
            with open(self.f) as f:
                d = json.load(f)
                self.assertEqual('win', d['os'])
                self.assertEqual('x86', d['processor'])
                self.assertEqual('windows', d['toolkit'])
                self.assertEqual(tempdir, d['topsrcdir'])
                self.assertEqual(mozconfig.name, d['mozconfig'])
                self.assertEqual(32, d['bits'])

    def test_fileobj(self):
        """
        Test that writing to a file-like object produces correct output.
        """
        s = StringIO()
        c = self._config(dict(
            OS_TARGET='WINNT',
            TARGET_CPU='i386',
            MOZ_WIDGET_TOOLKIT='windows',
        ))
        write_mozinfo(s, c)
        d = json.loads(s.getvalue())
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('windows', d['toolkit'])
        self.assertEqual(32, d['bits'])


if __name__ == '__main__':
    mozunit.main()
