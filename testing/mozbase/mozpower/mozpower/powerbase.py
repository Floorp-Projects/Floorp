# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import os

from .mozpowerutils import get_logger


class IPGExecutableMissingError(Exception):
    """IPGExecutableMissingError is raised when we cannot find
    the executable for Intel Power Gadget at the expected location.
    """
    pass


class PlatformUnsupportedError(Exception):
    """PlatformUnsupportedError is raised when we cannot find
    an expected IPG path for the OS being tested.
    """
    pass


class PowerBase(object):
    """PowerBase provides an interface for power measurement objects
    that depend on the os and cpu. When using this class as a base class
    the `initialize_power_measurements`, `finalize_power_measurements`,
    and `get_perfherder_data` functions must be implemented, otherwise
    a NotImplementedError will be raised.

    PowerBase should only be used as the base class for other
    classes and should not be instantiated directly. To enforce this
    restriction calling PowerBase's constructor will raise a
    NonImplementedError exception.

    ::

       from mozpower.powerbase import PowerBase

       try:
           pb = PowerBase()
       except NotImplementedError:
           print "PowerBase cannot be instantiated."

    """
    def __init__(self, logger_name='mozpower', os=None, cpu=None):
        """Initializes the PowerBase object.

        :param str logger_name: logging logger name. Defaults to 'mozpower'.
        :param str os: operating system being tested. Defaults to None.
        :param str cpu: cpu type being tested (either intel or arm64). Defaults to None.
        :raises: * NotImplementedError
        """
        if self.__class__ == PowerBase:
            raise NotImplementedError

        self._logger_name = logger_name
        self._logger = get_logger(logger_name)
        self._os = os
        self._cpu = cpu
        self._ipg_path = self.get_ipg_path()

    @property
    def ipg_path(self):
        return self._ipg_path

    @property
    def logger_name(self):
        return self._logger_name

    def initialize_power_measurements(self):
        """Starts power measurement gathering, must be implemented by subclass.

        :raises: * NotImplementedError
        """
        raise NotImplementedError

    def finalize_power_measurements(self, test_name='power-testing', **kwargs):
        """Stops power measurement gathering, must be implemented by subclass.

        :raises: * NotImplementedError
        """
        raise NotImplementedError

    def get_perfherder_data(self):
        """Returns the perfherder data output produced from the tests, must
        be implemented by subclass.

        :raises: * NotImplementedError
        """
        raise NotImplementedError

    def get_ipg_path(self):
        """Returns the path to where we expect to find Intel Power Gadget
        depending on the OS being tested. Raises a PlatformUnsupportedError
        if the OS being tested is unsupported, and raises a IPGExecutableMissingError
        if the Intel Power Gadget executable cannot be found at the expected
        location.

        :returns: str
        :raises: * IPGExecutableMissingError
                 * PlatformUnsupportedError
        """
        if self._cpu != "intel":
            return None

        if self._os == "darwin":
            exe_path = "/Applications/Intel Power Gadget/PowerLog"
        else:
            raise PlatformUnsupportedError(
                "%s platform currently not supported for Intel Power Gadget measurements" %
                self._os
            )
        if not os.path.exists(exe_path):
            raise IPGExecutableMissingError(
                "Intel Power Gadget is not installed, or cannot be found at the location %s" %
                exe_path
            )

        return exe_path
