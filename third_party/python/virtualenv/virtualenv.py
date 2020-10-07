# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
try:
    import importlib.util
    spec = importlib.util.spec_from_file_location('main', os.path.join(os.path.dirname(__file__), '__main__.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
except ImportError:
    import imp
    mod_info = imp.find_module('__main__', [os.path.dirname(__file__)])
    mod = imp.load_module('main', *mod_info)


# Fake zipfile module to make `mod.VersionedFindLoad` able to read the extracted
# files rather than the zipapp.
class fake_zipfile(object):
    class ZipFile(object):
        def __init__(self, path, mode):
            self._path = path
            self._mode = mode

        def open(self, path):
            #  The caller expects a raw file object, with no unicode handling.
            return open(os.path.join(self._path, path), self._mode + 'b')

        def close(self):
            pass

mod.zipfile = fake_zipfile


def run():
    with mod.VersionedFindLoad() as finder:
        sys.meta_path.insert(0, finder)
        finder._register_distutils_finder()
        from virtualenv.__main__ import run as run_virtualenv

        run_virtualenv()


if __name__ == "__main__":
    run()
