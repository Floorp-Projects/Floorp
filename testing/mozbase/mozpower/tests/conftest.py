from __future__ import absolute_import, print_function

import pytest

from mozpower.powerbase import PowerBase


@pytest.fixture(scope='function')
def powermeasurer():
    """Returns a testing subclass of the PowerBase class
    for testing.
    """
    class PowerMeasurer(PowerBase):
        pass
    return PowerMeasurer()
