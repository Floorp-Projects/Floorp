from __future__ import absolute_import, unicode_literals

import mozunit
import os
import pytest
import sys

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(1, mozharness_dir)

raptor_dir = os.path.join(os.path.dirname(here), 'raptor')
sys.path.insert(0, raptor_dir)

from utils import transform_platform


@pytest.mark.parametrize('platform', ['win', 'mac', 'linux64'])
def test_transform_platform(platform):
    transformed = transform_platform("mitmproxy-rel-bin-{platform}.manifest", platform)
    assert "{platform}" not in transformed
    if platform == 'mac':
        assert "osx" in transformed
    else:
        assert platform in transformed


def test_transform_platform_no_change():
    starting_string = "nothing-in-here-to-transform"
    assert transform_platform(starting_string, 'mac') == starting_string


@pytest.mark.parametrize('processor', ['x86_64', 'other'])
def test_transform_platform_processor(processor):
    transformed = transform_platform("string-with-processor-{x64}.manifest", 'win', processor)
    assert "{x64}" not in transformed
    if processor == 'x86_64':
        assert "_x64" in transformed


if __name__ == '__main__':
    mozunit.main()
