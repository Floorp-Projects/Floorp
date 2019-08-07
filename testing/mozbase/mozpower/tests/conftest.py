from __future__ import absolute_import, print_function

import os
import pytest
import tempfile

from mozpower.powerbase import PowerBase
from mozpower.intel_power_gadget import (
    IntelPowerGadget,
    IPGResultsHandler
)


@pytest.fixture(scope='function')
def powermeasurer():
    """Returns a testing subclass of the PowerBase class
    for testing.
    """
    class PowerMeasurer(PowerBase):
        pass
    return PowerMeasurer()


@pytest.fixture(scope='function')
def ipg_obj():
    """Returns an IntelPowerGadget object with the test
    output file path.
    """
    return IntelPowerGadget(
        'ipg-path',
        output_file_path=os.path.abspath(os.path.dirname(__file__)) +
        '/files/raptor-tp6-amazon-firefox_powerlog',
    )


@pytest.fixture(scope='function')
def ipg_rh_obj():
    """Returns an IPGResultsHandler object set up with the
    test files and cleans up the directory after the tests
    are complete.
    """
    base_path = os.path.abspath(os.path.dirname(__file__)) + '/files/'
    tmpdir = tempfile.mkdtemp()

    # Return the results handler for the test
    yield IPGResultsHandler(
        [
            base_path + 'raptor-tp6-amazon-firefox_powerlog_1_.txt',
            base_path + 'raptor-tp6-amazon-firefox_powerlog_2_.txt',
            base_path + 'raptor-tp6-amazon-firefox_powerlog_3_.txt',
        ],
        tmpdir
    )
