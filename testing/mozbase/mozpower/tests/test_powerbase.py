#!/usr/bin/env python

from __future__ import absolute_import

import mock
import mozunit
import pytest

from mozpower.powerbase import (
    PowerBase,
    IPGExecutableMissingError,
    PlatformUnsupportedError
)


def test_powerbase_intialization():
    """Tests that the PowerBase class correctly raises
    a NotImplementedError when attempting to instantiate
    it directly.
    """
    with pytest.raises(NotImplementedError):
        PowerBase()


def test_powerbase_missing_methods(powermeasurer):
    """Tests that trying to call PowerBase methods
    without an implementation in the subclass raises
    the NotImplementedError.
    """
    with pytest.raises(NotImplementedError):
        powermeasurer.initialize_power_measurements()

    with pytest.raises(NotImplementedError):
        powermeasurer.finalize_power_measurements()

    with pytest.raises(NotImplementedError):
        powermeasurer.get_perfherder_data()


def test_powerbase_get_ipg_path_mac(powermeasurer):
    """Tests that getting the IPG path returns the expected results.
    """
    powermeasurer._os = 'darwin'
    powermeasurer._cpu = 'not-intel'

    def os_side_effect(arg):
        """Used to get around the os.path.exists check in
        get_ipg_path which raises an IPGExecutableMissingError
        otherwise.
        """
        return True

    with mock.patch('os.path.exists', return_value=True) as m:
        m.side_effect = os_side_effect

        # None is returned when a non-intel based machine is
        # tested.
        ipg_path = powermeasurer.get_ipg_path()
        assert ipg_path is None

        # Check the path returned for mac intel-based machines.
        powermeasurer._cpu = 'intel'
        ipg_path = powermeasurer.get_ipg_path()
        assert ipg_path == "/Applications/Intel Power Gadget/PowerLog"


def test_powerbase_get_ipg_path_errors(powermeasurer):
    """Tests that the appropriate error is output when calling
    get_ipg_path with invalid/unsupported _os and _cpu entries.
    """

    powermeasurer._cpu = 'intel'
    powermeasurer._os = 'Not-An-OS'

    def os_side_effect(arg):
        """Used to force the error IPGExecutableMissingError to occur
        (in case a machine running these tests is a mac, and has intel
        power gadget installed).
        """
        return False

    with pytest.raises(PlatformUnsupportedError):
        powermeasurer.get_ipg_path()

    with mock.patch('os.path.exists', return_value=False) as m:
        m.side_effect = os_side_effect

        powermeasurer._os = 'darwin'
        with pytest.raises(IPGExecutableMissingError):
            powermeasurer.get_ipg_path()


if __name__ == '__main__':
    mozunit.main()
