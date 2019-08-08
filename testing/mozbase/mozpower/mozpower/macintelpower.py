# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import time

from .powerbase import PowerBase
from .intel_power_gadget import IntelPowerGadget, IPGResultsHandler


class MacIntelPower(PowerBase):
    """MacIntelPower is the OS and CPU dependent class for
    power measurement gathering on Mac Intel-based hardware.

    ::

       from mozpower.macintelpower import MacIntelPower

       # duration and output_file_path are used in IntelPowerGadget
       mip = MacIntelPower(ipg_measure_duration=600, output_file_path='power-testing')

       mip.initialize_power_measurements()
       # Run test...
       mip.finalize_power_measurements(test_name='raptor-test-name')

       perfherder_data = mip.get_perfherder_data()
    """
    def __init__(self, logger_name='mozpower', **kwargs):
        """Initializes the MacIntelPower object.

        :param str logger_name: logging logger name. Defaults to 'mozpower'.
        :param dict kwargs: optional keyword arguments passed to IntelPowerGadget.
        """
        PowerBase.__init__(self, logger_name=logger_name, os='darwin', cpu='intel')
        self.ipg = IntelPowerGadget(self.ipg_path, **kwargs)
        self.ipg_results_handler = None
        self.start_time = None
        self.end_time = None
        self.perfherder_data = {}

    def initialize_power_measurements(self):
        """Starts power measurement gathering through IntelPowerGadget.
        """
        self._logger.info("Initializing power measurements...")

        # Start measuring
        self.ipg.start_ipg()

        # Record start time to get an approximation of run time
        self.start_time = time.time()

    def finalize_power_measurements(self,
                                    test_name='power-testing',
                                    output_dir_path='',
                                    **kwargs):
        """Stops power measurement gathering through IntelPowerGadget, cleans the data,
        and produces partial perfherder formatted data that is stored in perfherder_data.

        :param str test_name: name of the test that was run.
        :param str output_dir_path: directory to store output files.
        :param dict kwargs: contains optional arguments to stop_ipg.
        """
        self._logger.info("Finalizing power measurements...")
        self.end_time = time.time()

        # Wait until Intel Power Gadget is done, then clean the data
        # and then format it
        self.ipg.stop_ipg(**kwargs)

        # Handle the results and format them to a partial perfherder format
        if not output_dir_path:
            output_dir_path = self.ipg.output_dir_path

        self.ipg_results_handler = IPGResultsHandler(
            self.ipg.output_files,
            output_dir_path,
            ipg_measure_duration=self.ipg.ipg_measure_duration,
            sampling_rate=self.ipg.sampling_rate,
            logger_name=self.logger_name
        )

        self.ipg_results_handler.clean_ipg_data()
        self.perfherder_data = self.ipg_results_handler.format_ipg_data_to_partial_perfherder(
            self.end_time - self.start_time, test_name
        )

    def get_perfherder_data(self):
        """Returns the perfherder data output that was produced.

        :returns: dict
        """
        return self.perfherder_data
