# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import platform
import os
import subprocess
import re

from .macintelpower import MacIntelPower
from .mozpowerutils import (
    average_summary,
    get_logger,
    sum_summary
)

OSCPU_COMBOS = {
    'darwin-intel': MacIntelPower,
}

SUMMARY_METHODS = {
    'utilization': average_summary,
    'power-usage': sum_summary,
    'default': sum_summary
}


class OsCpuComboMissingError(Exception):
    """OsCpuComboMissingError is raised when we cannot find
    a class responsible for the OS, and CPU combination that
    was detected.
    """
    pass


class MissingProcessorInfoError(Exception):
    """MissingProcessorInfoError is raised when we cannot find
    the processor information on the machine. This is raised when
    the file is missing (mentioned in the error message) or if
    an exception occurs when we try to gather the information
    from the file.
    """
    pass


class MozPower(object):
    """MozPower provides an OS and CPU independent interface
    for initializing, finalizing, and gathering power measurement
    data from OS+CPU combo-dependent measurement classes. The combo
    is detected automatically, and the correct class is instantiated
    based on the OSCPU_COMBOS list. If it cannot be found, an
    OsCpuComboMissingError will be raised.

    If a newly added power measurer does not have the required functions
    `initialize_power_measurements`, `finalize_power_measurements`,
    or `get_perfherder_data`, then a NotImplementedError will be
    raised.

    Android power measurements are currently not supported by this
    module.

    ::

       from mozpower import MozPower

       mp = MozPower(output_file_path='dir/power-testing')

       mp.initialize_power_measurements()
       # Run test...
       mp.finalize_power_measurements(test_name='raptor-test-name')

       perfherder_data = mp.get_perfherder_data()
    """
    def __init__(self,
                 android=False,
                 logger_name='mozpower',
                 output_file_path='power-testing',
                 **kwargs):
        """Initializes the MozPower object, detects OS and CPU (if not android),
        and instatiates the appropriate combo-dependent class for measurements.

        :param bool android: run android power measurer.
        :param str logger_name: logging logger name. Defaults to 'mozpower'.
        :param str output_file_path: path to where output files will be stored.
            Can include a prefix for the output files, i.e. 'dir/raptor-test'
            would output data to 'dir' with the prefix 'raptor-test'.
            Defaults to 'power-testing', the current directory using
            the prefix 'power-testing'.
        :param dict kwargs: optional keyword arguments passed to power measurer.
        :raises: * OsCpuComboMissingError
                 * NotImplementedError
        """
        self.measurer = None
        self._os = None
        self._cpu = None
        self._logger = get_logger(logger_name)

        if android:
            self._logger.error(
                "Android power usage measurer has not been implemented"
            )
            raise NotImplementedError
        else:
            self._os = self._get_os().lower()
            cpu = self._get_processor_info().lower()

            if 'intel' in cpu:
                self._cpu = 'intel'
            else:
                self._cpu = 'arm64'

        # OS+CPU combos are specified through strings such as 'darwin-intel'
        # for mac power measurement on intel-based machines. If none exist in
        # OSCPU_COMBOS, OsCpuComboMissingError will be raised.
        measurer = None
        oscpu_combo = '%s-%s' % (self._os, self._cpu)
        if oscpu_combo in OSCPU_COMBOS:
            measurer = OSCPU_COMBOS[oscpu_combo]
        else:
            raise OsCpuComboMissingError("Cannot find OS+CPU combo for %s" % oscpu_combo)

        if measurer:
            self._logger.info(
                "Intializing measurer %s for %s power measurements, see below for errors..." %
                (measurer.__name__, oscpu_combo)
            )
            self.measurer = measurer(
                logger_name=logger_name, output_file_path=output_file_path, **kwargs
            )

    def _get_os(self):
        """Returns the operating system of the machine being tested. platform.system()
        returns 'darwin' on MacOS, 'windows' on Windows, and 'linux' on Linux systems.

        :returns: str
        """
        return platform.system()

    def _get_processor_info(self):
        """Returns the processor model type of the machine being tested.
        Each OS has it's own way of storing this information. Raises
        MissingProcessorInfoError if we cannot get the processor info
        from the expected locations.

        :returns: str
        :raises: * MissingProcessorInfoError
        """
        model = ""

        if self._get_os() == "Windows":
            model = platform.processor()

        elif self._get_os() == "Darwin":
            proc_info_path = '/usr/sbin/sysctl'
            command = [proc_info_path, "-n", "machdep.cpu.brand_string"]

            if not os.path.exists(proc_info_path):
                raise MissingProcessorInfoError(
                    "Missing processor info file for darwin platform, "
                    "expecting it here %s" % proc_info_path
                )

            try:
                model = subprocess.check_output(command).strip()
            except subprocess.CalledProcessError as e:
                error_log = str(e)
                if e.output:
                    error_log = e.output.decode()
                raise MissingProcessorInfoError(
                    "Error while attempting to get darwin processor information "
                    "from %s (exists) with the command %s: %s" %
                    (proc_info_path, str(command), error_log)
                )

        elif self._get_os() == "Linux":
            proc_info_path = '/proc/cpuinfo'
            model_re = re.compile(r""".*model name\s+[:]\s+(.*)\s+""")

            if not os.path.exists(proc_info_path):
                raise MissingProcessorInfoError(
                    "Missing processor info file for linux platform, "
                    "expecting it here %s" % proc_info_path
                )

            try:
                with open(proc_info_path) as cpuinfo:
                    for line in cpuinfo:
                        if not line.strip():
                            continue
                        match = model_re.match(line)
                        if match:
                            model = match.group(1)
                if not model:
                    raise Exception(
                        "No 'model name' entries found in the processor info file"
                    )
            except Exception as e:
                raise MissingProcessorInfoError(
                    "Error while attempting to get linux processor information "
                    "from %s (exists): %s" % (proc_info_path, str(e))
                )

        return model

    def initialize_power_measurements(self, **kwargs):
        """Starts the power measurements by calling the power measurer's
        `initialize_power_measurements` function.

        :param dict kwargs: keyword arguments for power measurer initialization
            function if they are needed.
        """
        if self.measurer is None:
            return
        self.measurer.initialize_power_measurements(**kwargs)

    def finalize_power_measurements(self, **kwargs):
        """Stops the power measurements by calling the power measurer's
        `finalize_power_measurements` function.

        :param dict kwargs: keyword arguments for power measurer finalization
            function if they are needed.
        """
        if self.measurer is None:
            return
        self.measurer.finalize_power_measurements(**kwargs)

    def get_perfherder_data(self):
        """Returns the partial perfherder data output produced by the measurer.
        For a complete perfherder data blob, see get_full_perfherder_data.

        :returns: dict
        """
        if self.measurer is None:
            return
        return self.measurer.get_perfherder_data()

    def _summarize_values(self, datatype, values):
        """Summarizes the given values based on the type of the
        data. See SUMMARY_METHODS for the methods used for each
        known data type. Defaults to using the sum of the values
        when a data type cannot be found.

        Data type entries in SUMMARY_METHODS are case-sensitive.

        :param str datastype: the measurement type being summarized.
        :param list values: the values to summarize.
        :returns: float
        """
        if datatype not in SUMMARY_METHODS:
            self._logger.warning(
                "Missing summary method for data type %s, defaulting to sum" %
                datatype
            )
            datatype = 'default'

        summary_func = SUMMARY_METHODS[datatype]
        return summary_func(values)

    def get_full_perfherder_data(self, framework, lowerisbetter=True, alertthreshold=2.0):
        """Returns a list of complete perfherder data blobs compiled from the
        partial perfherder data blob returned from the measurer. Each key entry
        (measurement type) in the partial perfherder data is parsed into its
        own suite within a single perfherder data blob.

        For example, a partial perfherder data blob such as:

        ::

           {
               'utilization': {<perfherder_data>},
               'power-usage': {<perfherder_data>}
           }

        would produce two suites within a single perfherder data blobs -
        one for utilization, and one for power-usage.

        Note that the 'values' entry must exist, otherwise the measurement
        type is skipped. Furthermore, if 'name', 'unit', or 'type' is missing
        we default to:

        ::

           {
               'name': 'mozpower',
               'unit': 'mWh',
               'type': 'power'
           }

        Subtests produced for each sub-suite (measurement type), have the naming
        pattern: <measurement_type>-<measured_name>

        Utilization of cpu would have the following name: 'utilization-cpu'
        Power-usage for cpu has the following name: 'power-usage-cpu'

        :param str framework: name of the framework being tested, i.e. 'raptor'.
        :param bool lowerisbetter: if set to true, low values are better than high ones.
        :param float alertthreshold: determines the crossing threshold at
            which an alert is generated.
        :returns: dict
        """
        if self.measurer is None:
            return

        # Get the partial data, and the measurers name for
        # logging purposes.
        partial_perfherder_data = self.get_perfherder_data()
        measurer_name = self.measurer.__class__.__name__

        suites = []
        perfherder_data = {
            'framework': {'name': framework},
            'suites': suites
        }

        for measurement_type in partial_perfherder_data:
            self._logger.info("Summarizing %s data" % measurement_type)
            dataset = partial_perfherder_data[measurement_type]

            # Skip this measurement type if the 'values' entry
            # doesn't exist, and output a warning.
            if 'values' not in dataset:
                self._logger.warning(
                    "Missing 'values' entry in partial perfherder data for measurement type %s "
                    "obtained from %s. This measurement type will not be processed." %
                    (measurement_type, measurer_name)
                )
                continue

            # Get the settings, if they exist, otherwise output
            # a warning and use a default entry.
            settings = {
                'test': 'mozpower',
                'unit': 'mWh',
                'type': 'power'
            }

            for setting in settings:
                if setting in dataset:
                    settings[setting] = dataset[setting]
                else:
                    self._logger.warning(
                        "Missing '%s' entry in partial perfherder data for measurement type %s "
                        "obtained from %s, using %s as the default" %
                        (setting, measurement_type, measurer_name, settings[setting])
                    )

            subtests = []
            suite = {
                'name': "%s-%s" % (settings['test'], measurement_type),
                'type': settings['type'],
                'value': 0,
                'subtests': subtests,
                'lowerIsBetter': lowerisbetter,
                'unit': settings['unit'],
                'alertThreshold': alertthreshold,
            }

            # Parse the 'values' entries into subtests
            values = []
            for measure in dataset['values']:
                value = dataset['values'][measure]
                subtest = {
                    'name': '%s-%s' % (measurement_type, measure),
                    'value': float(value),
                    'lowerIsBetter': lowerisbetter,
                    'alertThreshold': alertthreshold,
                    'unit': settings['unit']
                }
                values.append(value)
                subtests.append(subtest)

            # Summarize the data based on the measurement type
            if len(values) > 0:
                suite['value'] = self._summarize_values(measurement_type, values)
            suites.append(suite)

        return perfherder_data
