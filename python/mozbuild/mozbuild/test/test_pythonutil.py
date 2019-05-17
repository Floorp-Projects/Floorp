# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozbuild.pythonutil import iter_modules_in_path
from mozunit import main
import os


def test_iter_modules_in_path():
    tests_path = os.path.normcase(os.path.dirname(__file__))
    paths = list(iter_modules_in_path(tests_path))
    assert set(paths) == set([
        os.path.join(os.path.abspath(tests_path), '__init__.py'),
        os.path.join(os.path.abspath(tests_path), 'test_pythonutil.py'),
    ])


if __name__ == '__main__':
    main()
