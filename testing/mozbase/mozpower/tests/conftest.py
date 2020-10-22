from __future__ import absolute_import, print_function

import mock
import os
import pytest
import tempfile
import time

from mozpower import MozPower
from mozpower.macintelpower import MacIntelPower
from mozpower.powerbase import PowerBase
from mozpower.intel_power_gadget import (
    IntelPowerGadget,
    IPGResultsHandler
)


def os_side_effect(*args, **kwargs):
    """Used as a side effect to os.path.exists when
    checking if the Intel Power Gadget executable exists.
    """
    return True


def subprocess_side_effect(*args, **kwargs):
    """Used as a side effect when running the Intel Power
    Gadget tool.
    """
    time.sleep(1)


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


@pytest.fixture(scope='function')
def macintelpower_obj():
    """Returns a MacIntelPower object with subprocess.check_output
    and os.path.exists calls patched with side effects.
    """
    with mock.patch('subprocess.check_output') as subprocess_mock:
        with mock.patch('os.path.exists') as os_mock:
            subprocess_mock.side_effect = subprocess_side_effect
            os_mock.side_effect = os_side_effect

            yield MacIntelPower(ipg_measure_duration=2)


@pytest.fixture(scope='function')
def mozpower_obj():
    """Returns a MozPower object with subprocess.check_output
    and os.path.exists calls patched with side effects.
    """
    with mock.patch.object(MozPower, '_get_os', return_value='Darwin') as _, \
            mock.patch.object(
                MozPower, '_get_processor_info', return_value='GenuineIntel'
            ) as _, \
            mock.patch.object(
                MacIntelPower, 'get_ipg_path', return_value='/'
            ) as _, \
            mock.patch('subprocess.check_output') as subprocess_mock, \
            mock.patch('os.path.exists') as os_mock:
        subprocess_mock.side_effect = subprocess_side_effect
        os_mock.side_effect = os_side_effect

        yield MozPower(ipg_measure_duration=2)
