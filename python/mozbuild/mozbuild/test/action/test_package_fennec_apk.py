# -*- coding: utf-8 -*-

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import unicode_literals

import os
import unittest

import mozunit

from mozbuild.action.package_fennec_apk import (
    package_fennec_apk as package,
)
from mozpack.mozjar import JarReader
import mozpack.path as mozpath


test_data_path = mozpath.abspath(mozpath.dirname(__file__))
test_data_path = mozpath.join(test_data_path, 'data', 'package_fennec_apk')


def data(name):
    return os.path.join(test_data_path, name)


class TestPackageFennecAPK(unittest.TestCase):
    """
    Unit tests for package_fennec_apk.py.
    """

    def test_arguments(self):
        # Language repacks take updated resources from an ap_ and pack them
        # into an apk.  Make sure the second input overrides the first.
        jarrer = package(inputs=[],
                         omni_ja=data('omni.ja'),
                         classes_dex=data('classes.dex'),
                         assets_dirs=[data('assets')],
                         lib_dirs=[data('lib')],
                         root_files=[data('root_file.txt')])

        # omni.ja ends up in assets/omni.ja.
        self.assertEquals(jarrer['assets/omni.ja'].open().read().strip(), 'omni.ja')

        # Everything else is in place.
        for name in ('classes.dex',
                     'assets/asset.txt',
                     'lib/lib.txt',
                     'root_file.txt'):
            self.assertEquals(jarrer[name].open().read().strip(), name)

    def test_inputs(self):
        # Language repacks take updated resources from an ap_ and pack them
        # into an apk.  In this case, the first input is the original package,
        # the second input the update ap_.  Make sure the second input
        # overrides the first.
        jarrer = package(inputs=[data('input2.apk'), data('input1.ap_')])

        files1 = JarReader(data('input1.ap_')).entries.keys()
        files2 = JarReader(data('input2.apk')).entries.keys()
        for name in files2:
            self.assertTrue(name in files1 or
                            jarrer[name].open().read().startswith('input2/'))
        for name in files1:
            self.assertTrue(jarrer[name].open().read().startswith('input1/'))


if __name__ == '__main__':
    mozunit.main()
