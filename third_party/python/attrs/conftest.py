from __future__ import absolute_import, division, print_function

import sys

import pytest

from hypothesis import HealthCheck, settings

from attr._compat import PYPY


def pytest_configure(config):
    # HealthCheck.too_slow causes more trouble than good -- especially in CIs.
    settings.register_profile(
        "patience", settings(suppress_health_check=[HealthCheck.too_slow])
    )
    settings.load_profile("patience")


@pytest.fixture(scope="session")
def C():
    """
    Return a simple but fully featured attrs class with an x and a y attribute.
    """
    import attr

    @attr.s
    class C(object):
        x = attr.ib()
        y = attr.ib()

    return C


collect_ignore = []
if sys.version_info[:2] < (3, 6):
    collect_ignore.extend(
        ["tests/test_annotations.py", "tests/test_init_subclass.py"]
    )
elif PYPY:  # FIXME: Currently our tests fail on pypy3. See #509
    collect_ignore.extend(["tests/test_annotations.py"])
