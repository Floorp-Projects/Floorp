# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozpack.copier import FileCopier, FileRegistry
from mozpack.packager.formats import FlatFormatter, JarFormatter, OmniJarFormatter
from mozpack.packager.unpack import unpack_to_registry
from mozpack.test.test_files import TestWithTmpDir
from mozpack.test.test_packager_formats import CONTENTS, fill_formatter, get_contents


class TestUnpack(TestWithTmpDir):
    maxDiff = None

    @staticmethod
    def _get_copier(cls):
        copier = FileCopier()
        formatter = cls(copier)
        fill_formatter(formatter, CONTENTS)
        return copier

    @classmethod
    def setUpClass(cls):
        cls.contents = get_contents(
            cls._get_copier(FlatFormatter), read_all=True, mode="rb"
        )

    def _unpack_test(self, cls):
        # Format a package with the given formatter class
        copier = self._get_copier(cls)
        copier.copy(self.tmpdir)

        # Unpack that package. Its content is expected to match that of a Flat
        # formatted package.
        registry = FileRegistry()
        unpack_to_registry(self.tmpdir, registry, getattr(cls, "OMNIJAR_NAME", None))
        self.assertEqual(
            get_contents(registry, read_all=True, mode="rb"), self.contents
        )

    def test_flat_unpack(self):
        self._unpack_test(FlatFormatter)

    def test_jar_unpack(self):
        self._unpack_test(JarFormatter)

    @staticmethod
    def _omni_foo_formatter(name):
        class OmniFooFormatter(OmniJarFormatter):
            OMNIJAR_NAME = name

            def __init__(self, registry):
                super(OmniFooFormatter, self).__init__(registry, name)

        return OmniFooFormatter

    def test_omnijar_unpack(self):
        self._unpack_test(self._omni_foo_formatter("omni.foo"))

    def test_omnijar_subpath_unpack(self):
        self._unpack_test(self._omni_foo_formatter("bar/omni.foo"))


if __name__ == "__main__":
    mozunit.main()
