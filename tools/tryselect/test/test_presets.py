# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import mozunit
import pytest


@pytest.fixture(scope='session')
def run_mach():
    import mach_bootstrap
    from mach.config import ConfigSettings
    from tryselect.tasks import build

    mach = mach_bootstrap.bootstrap(build.topsrcdir)

    def inner(args):
        mach.settings = ConfigSettings()
        return mach.run(args)

    return inner


def test_shared_presets(run_mach, shared_name, shared_preset):
    """This test makes sure that we don't break any of the in-tree presets when
    renaming/removing variables in any of the selectors.
    """
    assert 'description' in shared_preset
    assert 'selector' in shared_preset

    selector = shared_preset['selector']
    if selector == 'fuzzy':
        assert 'query' in shared_preset
        assert isinstance(shared_preset['query'], list)

    # Run the preset and assert there were no exceptions.
    assert run_mach(['try', '--no-push', '--preset', shared_name]) == 0


if __name__ == '__main__':
    mozunit.main()
