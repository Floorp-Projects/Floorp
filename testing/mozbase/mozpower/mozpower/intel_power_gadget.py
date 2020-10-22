# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals

import csv
import os
import subprocess
import re
import time
import threading

from .mozpowerutils import get_logger


class IPGTimeoutError(Exception):
    """IPGTimeoutError is raised when we cannot stop Intel Power
    Gadget from running. One possble cause of this is not calling
    `stop_ipg` through a `finalize_power_measurements` call. The
    other possiblity is that IPG failed to stop.
    """
    pass


class IPGMissingOutputFileError(Exception):
    """IPGMissingOutputFile is raised when a file path is given
    to _clean_ipg_file but it does not exist or cannot be found
    at the expected location.
    """
    pass


class IPGEmptyFileError(Exception):
    """IPGEmptyFileError is raised when a file path is given
    to _clean_ipg_file and it exists but it is empty (contains
    no results to clean).
    """
    pass


class IPGUnknownValueTypeError(Exception):
    """IPGUnknownValueTypeError is raised when a value within a
    given results file (that was cleaned) cannot be converted to
    its column's expected data type.
    """
    pass


class IntelPowerGadget(object):
    """IntelPowerGadget provides methods for using Intel Power Gadget
    to measure power consumption.

    ::

       from mozpower.intel_power_gadget import IntelPowerGadget

       # On Mac, the ipg_exe_path should point to '/Applications/Intel Power Gadget/PowerLog'
       ipg = IntelPowerGadget(ipg_exe_path, ipg_measure_duration=600, sampling_rate=500)

       ipg.start_ipg()

       # Run tests...

       ipg.stop_ipg()

       # Now process results with IPGResultsHandler by passing it
       # ipg.output_files, ipg.output_file_ext, ipg.ipg_measure_duration,
       # and ipg.sampling_rate.
    """
    def __init__(self,
                 exe_file_path,
                 ipg_measure_duration=10,
                 sampling_rate=1000,
                 output_file_ext='.txt',
                 file_counter=1,
                 output_file_path='powerlog',
                 logger_name='mozpower'):
        """Initializes the IntelPowerGadget object.

        :param str exe_file_path: path to Intel Power Gadget 'PowerLog' executable.
        :param int ipg_measure_duration: duration to run IPG for in seconds.
            This does not dictate when the tools shuts down. It only stops when stop_ipg
            is called in case the test runs for a non-deterministic amount of time. The
            IPG executable requires a duration to be supplied. The machinery in place is
            to ensure that IPG keeps recording as long as the experiment runs, so
            multiple files may result, which is handled in
            IPGResultsHandler._combine_cumulative_rows. Defaults to 10s.
        :param int sampling_rate: sampling rate of measurements in milliseconds.
            Defaults to 1000ms.
        :param output_file_ext: file extension of data being output. Defaults to '.txt'.
        :param int file_counter: dictates the start of the file numbering (used
            when test time exceeds duration value). Defaults to 0.
        :param str output_file_path: path to the output location combined
            with the output file prefix. Defaults to current working directory using the
            prefix 'powerlog'.
        :param str logger_name: logging logger name. Defaults to 'mozpower'.
        """
        self._logger = get_logger(logger_name)

        # Output-specific settings
        self._file_counter = file_counter
        self._output_files = []
        self._output_file_path = output_file_path
        self._output_file_ext = output_file_ext
        self._output_dir_path, self._output_file_prefix = os.path.split(self._output_file_path)

        # IPG-specific settings
        self._ipg_measure_duration = ipg_measure_duration  # in seconds
        self._sampling_rate = sampling_rate  # in milliseconds
        self._exe_file_path = exe_file_path

        # Setup thread for measurement gathering
        self._thread = threading.Thread(
            target=self.run, args=(exe_file_path, ipg_measure_duration)
        )
        self._thread.daemon = True
        self._running = False

    @property
    def output_files(self):
        """Returns the list of files produced from running IPG.

        :returns: list
        """
        return self._output_files

    @property
    def output_file_ext(self):
        """Returns the extension of the files produced by IPG.

        :returns: str
        """
        return self._output_file_ext

    @property
    def output_file_prefix(self):
        """Returns the prefix of the files produces by IPG.

        :returns: str
        """
        return self._output_file_prefix

    @property
    def output_dir_path(self):
        """Returns the output directory of the files produced by IPG.

        :returns: str
        """
        return self._output_dir_path

    @property
    def sampling_rate(self):
        """Returns the specified sampling rate.

        :returns: int
        """
        return self._sampling_rate

    @property
    def ipg_measure_duration(self):
        """Returns the IPG measurement duration (see __init__ for description
        of what this value does).

        :returns: int
        """
        return self._ipg_measure_duration

    def start_ipg(self):
        """Starts the thread which runs IPG to start gathering measurements.
        """
        self._logger.info("Starting IPG thread")
        self._stop = False
        self._thread.start()

    def stop_ipg(self, wait_interval=10, timeout=200):
        """Stops the thread which runs IPG and waits for it to finish it's work.

        :param int wait_interval: interval time (in seconds) at which to check if
            IPG is finished.
        :param int timeout: time to wait until _wait_for_ipg hits a time out,
            in seconds.
        """
        self._logger.info("Stopping IPG thread")
        self._stop = True
        self._wait_for_ipg(wait_interval=wait_interval, timeout=timeout)

    def _wait_for_ipg(self, wait_interval=10, timeout=200):
        """Waits for IPG to finish it's final dataset.

        :param int wait_interval: interval time (in seconds) at which to check if
            IPG is finished.
        :param int timeout: time to wait until this method hits a time out,
            in seconds.
        :raises: * IPGTimeoutError
        """
        timeout_stime = time.time()
        while self._running and (time.time() - timeout_stime) < timeout:
            self._logger.info("Waiting %s sec for Intel Power Gadget to stop" % wait_interval)
            time.sleep(wait_interval)
        if self._running:
            raise IPGTimeoutError("Timed out waiting for IPG to stop")

    def _get_output_file_path(self):
        """Returns the next output file name to be used. Starts at 1 and increases
        at each successive call. Used when the test duration exceeds the specified
        duration through ipg_meaure_duration.

        :returns: str
        """
        self._output_file_path = os.path.join(
            self._output_dir_path,
            '%s_%s_%s' % (self._output_file_prefix, self._file_counter, self._output_file_ext)
        )
        self._file_counter += 1
        self._logger.info(
            "Creating new file for IPG data measurements: %s" % self._output_file_path
        )
        return self._output_file_path

    def run(self, exe_file_path, ipg_measure_duration):
        """Runs the IPG measurement gatherer. While stop has not been set to True,
        it continuously gathers measurements into separate files that are merged
        by IPGResultsHandler once the output_files are passed to it.

        :param str exe_file_path: file path of where to find the IPG executable.
        :param int ipg_measure_duration: time to gather measurements for.
        """
        self._logger.info("Starting to gather IPG measurements in thread")
        self._running = True

        while not self._stop:
            outname = self._get_output_file_path()
            self._output_files.append(outname)

            try:
                subprocess.check_output([
                    exe_file_path,
                    '-duration', str(ipg_measure_duration),
                    '-resolution', str(self._sampling_rate),
                    '-file', outname
                ], stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                error_log = str(e)
                if e.output:
                    error_log = e.output.decode()
                self._logger.critical(
                    "Error while running Intel Power Gadget: %s" % error_log
                )

        self._running = False


class IPGResultsHandler(object):
    """IPGResultsHandler provides methods for cleaning and formatting
    the files that were created by IntelPowerGadget.

    ::

       from mozpower.intel_power_gadget import IntelPowerGadget, IPGResultsHandler

       ipg = IntelPowerGadget(ipg_exe_path, ipg_measure_duration=600, sampling_rate=500)

       # Run tests - call start_ipg and stop_ipg...

       ipg_rh = IPGResultsHandler(
            ipg.output_files,
            ipg.output_dir_path,
            ipg_measure_duration=ipg.ipg_measure_duration,
            sampling_rate=ipg.sampling_rate
        )

       cleaned_data = ipg_rh.clean_ipg_data()
       # you can also get the data from results after calling clean_ipg_data
       cleaned_data = ipg_rh.results

       perfherder_data = ipg_rh.format_ipg_data_to_partial_perfherder(
            experiment_duration,
            test_name
       )
       # you can also get the perfherder data from summarized_results
       # after calling format_ipg_data_to_partial_perfherder
       perfherder_data = ipg_rh.summarized_results
    """
    def __init__(self,
                 output_files,
                 output_dir_path,
                 ipg_measure_duration=10,
                 sampling_rate=1000,
                 logger_name='mozpower'):
        """Initializes the IPGResultsHandler object.

        :param list output_files: files output by IntelPowerGadget containing
            the IPG data.
        :param str output_dir_path: location to store cleaned files and merged data.
        :param int ipg_measure_duration: length of time that each IPG measurement lasted
            in seconds (see IntelPowerGadget for more information on this argument).
            Defaults to 10s.
        :param int sampling_rate: sampling rate of the measurements in milliseconds.
            Defaults to 1000ms.
        :param str logger_name: logging logger name. Defaults to 'mozpower'.
        """
        self._logger = get_logger(logger_name)
        self._results = {}
        self._summarized_results = {}
        self._cleaned_files = []
        self._csv_header = None
        self._merged_output_path = None
        self._output_file_prefix = None
        self._output_file_ext = None

        self._sampling_rate = sampling_rate
        self._ipg_measure_duration = ipg_measure_duration
        self._output_files = output_files
        self._output_dir_path = output_dir_path

        if self._output_files:
            # Gather the output file extension, and prefix
            # for the cleaned files, and the merged file.
            single_file = self._output_files[0]
            _, file = os.path.split(single_file)
            self._output_file_ext = '.' + file.split('.')[-1]

            # This prefix detection depends on the path names created
            # by _get_output_file_path.
            integer_re = re.compile(r"""(.*)_[\d+]_%s""" % self._output_file_ext)
            match = integer_re.match(file)
            if match:
                self._output_file_prefix = match.group(1)
            else:
                self._output_file_prefix = file.split('_')[0]
                self._logger.warning(
                    "Cannot find output file prefix from output file name %s"
                    "using the following prefix: %s" % (file, self._output_file_prefix)
                )

    @property
    def results(self):
        """Returns the cleaned IPG data in the form of a dict.
        Each key is a measurement name with the values being the list
        of measurements. All value lists are sorted in increasing time.

        :returns: dict
        """
        return self._results

    @property
    def summarized_results(self):
        """Returns IPG data in the form of a dict that is formatted
        into a perfherder data blob.

        :returns: dict
        """
        return self._summarized_results

    @property
    def cleaned_files(self):
        """Returns a list of cleaned IPG data files that were output
        after running clean_ipg_data.

        :returns: list
        """
        return self._cleaned_files

    @property
    def merged_output_path(self):
        """Returns the path to the cleaned, and merged output file.

        :returns: str
        """
        return self._merged_output_path

    def _clean_ipg_file(self, file):
        """Cleans an individual IPG file and writes out a cleaned file.

        An uncleaned file looks like this (this contains a partial
        sample of the time series data):
        ```
        "System Time","RDTSC","Elapsed Time (sec)"
        "12:11:05:769","61075218263548","   2.002"
        "12:11:06:774","61077822279584","   3.007"
        "12:11:07:778","61080424421708","   4.011"
        "12:11:08:781","61083023535972","   5.013"
        "12:11:09:784","61085623402302","   6.016"

        "Total Elapsed Time (sec) = 10.029232"
        "Measured RDTSC Frequency (GHz) = 2.592"

        "Cumulative Processor Energy_0 (Joules) = 142.337524"
        "Cumulative Processor Energy_0 (mWh) = 39.538201"
        "Average Processor Power_0 (Watt) = 14.192265"

        "Cumulative IA Energy_0 (Joules) = 121.888000"
        "Cumulative IA Energy_0 (mWh) = 33.857778"
        "Average IA Power_0 (Watt) = 12.153273"

        "Cumulative DRAM Energy_0 (Joules) = 7.453308"
        "Cumulative DRAM Energy_0 (mWh) = 2.070363"
        "Average DRAM Power_0 (Watt) = 0.743158"

        "Cumulative GT Energy_0 (Joules) = 0.079834"
        "Cumulative GT Energy_0 (mWh) = 0.022176"
        "Average GT Power_0 (Watt) = 0.007960"
        ```

        While a cleaned file looks like:
        ```
        System Time,RDTSC,Elapsed Time (sec)
        12:11:05:769,61075218263548,   2.002
        12:11:06:774,61077822279584,   3.007
        12:11:07:778,61080424421708,   4.011
        12:11:08:781,61083023535972,   5.013
        12:11:09:784,61085623402302,   6.016
        ```

        The first portion of the file before the '"Total Elapsed Time' entry is
        considered as the time series which is captured as a dict in the returned
        results.

        All the text starting from '"Total Elapsed Time' is removed from the file and
        is considered as a summary of the experiment (which is returned). This
        portion is ignored in any other processing done by IPGResultsHandler,
        and is not saved, unlike the returned results value.

        Note that the new lines are removed from the summary that is returned
        (as well as the results).

        :param str file: file to clean.
        :returns: a tuple of (dict, list, str) for
            (results, summary, clean_output_path)
            see method comments above for information on what
            this output contains.
        :raises: * IPGMissingOutputFileError
                 * IPGEmptyFileError
        """
        self._logger.info("Cleaning IPG data file %s" % file)

        txt = ''
        if os.path.exists(file):
            with open(file, 'r') as f:
                txt = f.read()
        else:
            # This should never happen, so prevent IPGResultsHandler
            # from continuing to clean files if it does occur.
            raise IPGMissingOutputFileError(
                "The following file does not exist so it cannot be cleaned: %s " % file
            )

        if txt == '':
            raise IPGEmptyFileError(
                "The following file is empty so it cannot be cleaned: %s" % file
            )

        # Split the time series data from the summary
        tseries, summary = re.split('"Total Elapsed Time', txt)

        # Clean the summary
        summary = '"Total Elapsed Time' + summary
        summary = [
            line for line in re.split(r"""\n""", summary.replace('\r', '')) if line
        ]

        # Clean the time series data, store the clean rows to write out later,
        # and format the rows into a dict entry for each measure.
        results = {}
        clean_rows = []
        csv_header = None
        for c, row in enumerate(
            csv.reader(tseries.split('\n'), quotechar=str('"'), delimiter=str(','))
        ):
            if not row:
                continue

            # Make sure we don't have any bad line endings
            # contaminating the cleaned rows.
            fmt_row = [
                val.replace('\n', '').
                replace('\t', '').
                replace('\r', '').
                replace('\\n', '').
                strip()
                for val in row
            ]

            if not fmt_row or not any(fmt_row):
                continue

            clean_rows.append(fmt_row)
            if c == 0:
                csv_header = fmt_row
                for col in fmt_row:
                    results[col] = []
                continue
            for i, col in enumerate(fmt_row):
                results[csv_header[i]].append(col)

        # Write out the cleaned data and check to make sure
        # the csv header hasn't changed mid-experiment
        _, fname = os.path.split(file)
        clean_output_path = os.path.join(
            self._output_dir_path,
            fname.replace(
                self._output_file_ext, '_clean.%s' % self._output_file_ext.replace('.', '')
            )
        )
        self._logger.info("Writing cleaned IPG results to %s" % clean_output_path)
        try:
            with open(clean_output_path, 'w') as csvfile:
                writer = csv.writer(csvfile)
                for count, row in enumerate(clean_rows):
                    if count == 0:
                        if self._csv_header is None:
                            self._csv_header = row
                        elif self._csv_header != row:
                            self._logger.warning(
                                "CSV Headers from IPG data have changed during the experiment "
                                "expected: %s; got: %s" % (str(self._csv_header), str(row))
                            )
                    writer.writerow(row)
        except Exception as e:
            self._logger.warning(
                "Could not write out cleaned results of %s to %s due to the following error"
                ", skipping this step: %s" % (file, clean_output_path, str(e))
            )

        # Check to make sure the expected number of samples
        # exist in the file and that the columns have the correct
        # data type.
        column_datatypes = {
            'System Time': str,
            'RDTSC': int,
            'default': float
        }
        expected_samples = int(self._ipg_measure_duration/(float(self._sampling_rate)/1000))
        for key in results:
            if len(results[key]) != expected_samples:
                self._logger.warning(
                    "Unexpected number of samples in %s for column %s - "
                    "expected: %s, got: %s" %
                    (clean_output_path, key, expected_samples, len(results[key]))
                )

            dtype = column_datatypes['default']
            if key in column_datatypes:
                dtype = column_datatypes[key]

            for val in results[key]:
                try:
                    # Check if converting from a string to its expected data
                    # type works, if not, it's not the expected data type.
                    dtype(val)
                except ValueError as e:
                    raise IPGUnknownValueTypeError(
                        "Cleaned file %s entry %s in column %s has unknown type "
                        "instead of the expected type %s - data corrupted, "
                        "cannot continue: %s" %
                        (clean_output_path, str(val), key, dtype.__name__, str(e))
                    )

        # Check to make sure that IPG measured for the expected
        # amount of time.
        etime = 'Elapsed Time (sec)'
        if etime in results:
            total_time = int(float(results[etime][-1]))
            if total_time != self._ipg_measure_duration:
                self._logger.warning(
                    "Elapsed time found in file %s is different from expected length - "
                    "expected: %s, got: %s" %
                    (clean_output_path, self._ipg_measure_duration, total_time)
                )
        else:
            self._logger.warning("Missing 'Elapsed Time (sec)' in file %s" % clean_output_path)

        return results, summary, clean_output_path

    def _combine_cumulative_rows(self, cumulatives):
        """Combine cumulative rows from multiple IPG files into
        a single time series.

        :param list cumulatives: list of cumulative time series collected
            over time, on for each file. Must be ordered in increasing time.
        :returns: list
        """
        combined_cumulatives = []

        val_mod = 0
        for count, cumulative in enumerate(cumulatives):
            # Add the previous file's maximum cumulative value
            # to the current one
            mod_cumulative = [
                val_mod + float(val)
                for val in cumulative
            ]
            val_mod = mod_cumulative[-1]
            combined_cumulatives.extend(mod_cumulative)

        return combined_cumulatives

    def clean_ipg_data(self):
        """Cleans all IPG files, or data, that was produced by an IntelPowerGadget object.
        Returns a dict containing the merged data from all files.

        :returns: dict
        """
        self._logger.info("Cleaning IPG data...")

        if not self._output_files:
            self._logger.warning("No IPG files to clean.")
            return
        # If this is called a second time for the same set of files,
        # then prevent it from duplicating the cleaned file entries.
        if self._cleaned_files:
            self._cleaned_files = []

        # Clean each file individually and gather the results
        all_results = []
        for file in self._output_files:
            results, summary, output_file_path = self._clean_ipg_file(file)
            self._cleaned_files.append(output_file_path)
            all_results.append(results)

        # Merge all the results into a single result
        combined_results = {}
        for measure in all_results[0]:
            lmeasure = measure.lower()
            if 'cumulative' not in lmeasure and 'elapsed time' not in lmeasure:
                # For measures which are not cumulative, or elapsed time,
                # combine them without changing the data.
                for count, result in enumerate(all_results):
                    if 'system time' in lmeasure or 'rdtsc' in lmeasure:
                        new_results = result[measure]
                    else:
                        new_results = [float(val) for val in result[measure]]

                    if count == 0:
                        combined_results[measure] = new_results
                    else:
                        combined_results[measure].extend(new_results)
            else:
                # For cumulative, and elapsed time measures, we need to
                # modify all values - see _combine_cumulative_rows for
                # more information on this procedure.
                cumulatives = [
                    result[measure]
                    for result in all_results
                ]
                self._logger.info("Combining cumulative rows for '%s' measure" % measure)
                combined_results[measure] = self._combine_cumulative_rows(cumulatives)

        # Write merged results to a new file
        merged_output_path = os.path.join(
            self._output_dir_path,
            '%s_merged.%s' % (self._output_file_prefix, self._output_file_ext.replace('.', ''))
        )

        self._merged_output_path = merged_output_path
        self._logger.info("Writing merged IPG results to %s" % merged_output_path)
        try:
            with open(merged_output_path, 'w') as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(self._csv_header)

                # Use _csv_header list to keep the header ordering
                # the same as the cleaned and raw files.
                first_key = list(combined_results.keys())[0]
                for row_count, _ in enumerate(combined_results[first_key]):
                    row = []
                    for measure in self._csv_header:
                        row.append(combined_results[measure][row_count])
                    writer.writerow(row)
        except Exception as e:
            self.merged_output_path = None
            self._logger.warning(
                "Could not write out merged results to %s due to the following error"
                ", skipping this step: %s" % (merged_output_path, str(e))
            )

        # Check that the combined results have the expected number of samples.
        expected_samples = int(self._ipg_measure_duration/(float(self._sampling_rate)/1000))
        combined_expected_samples = len(self._cleaned_files) * expected_samples
        for key in combined_results:
            if len(combined_results[key]) != combined_expected_samples:
                self._logger.warning(
                    "Unexpected number of merged samples in %s for column %s - "
                    "expected: %s, got: %s" %
                    (merged_output_path, key, combined_expected_samples, len(results[key]))
                )

        self._results = combined_results
        return self._results

    def format_ipg_data_to_partial_perfherder(self, duration, test_name):
        """Format the merged IPG data produced by clean_ipg_data into a
        partial/incomplete perfherder data blob. Returns the perfherder
        data and stores it in _summarized_results.

        This perfherder data still needs more information added to it before it
        can be validated against the perfherder schema. The entries returned
        through here are missing many required fields. Furthermore, the 'values'
        entries need to be formatted into 'subtests'.

        Here is a sample of a complete perfherder data which uses
        the 'utilization' results within the subtests:
        ```
        {
            "framework": {"name": "raptor"},
            "type": "power",
            "unit": "mWh"
            "suites": [
                {
                    "name": "raptor-tp6-amazon-firefox-power",
                    "lowerIsBetter": true,
                    "alertThreshold": 2.0,
                    "subtests": [
                        {
                            "lowerIsBetter": true,
                            "unit": "%",
                            "name": "raptor-tp6-youtube-firefox-power-cpu",
                            "value": 14.409090909090908,
                            "alertThreshold": 2.0
                        },
                        {
                            "lowerIsBetter": true,
                            "unit": "%",
                            "name": "raptor-tp6-youtube-firefox-power-gpu",
                            "value": 20.1,
                            "alertThreshold": 2.0
                        },
                    ]
                }
            ]
        }
        ```

        To obtain data that is formatted to a complete perfherder data blob,
        see get_complete_perfherder_data in MozPower.

        :param float duration: the actual duration of the test in case some
            data needs to be cut off from the end.
        :param str test_name: the name of the test.
        :returns: dict
        """
        self._logger.info("Formatting cleaned IPG data into partial perfherder data")

        if not self._results:
            self._logger.warning(
                "No merged results found - cannot format data to perfherder format."
            )
            return

        def replace_measure_name(name):
            """Replaces the long IPG names with shorter versions.
            Returns the given name if no conversions exist.

            :param str name: name of the entry to replace.
            :returns: str
            """
            lname = name.lower()
            if 'ia ' in lname:
                return 'processor-cores'
            elif 'processor ' in lname:
                return 'processor-package'
            elif 'gt ' in lname:
                return 'gpu'
            elif 'dram ' in lname:
                return 'dram'
            else:
                return name

        # Cut out entries which surpass the test duration.
        # Occurs when IPG continues past the call to stop it.
        cut_results = self._results
        if duration:
            cutoff_index = 0
            for count, etime in enumerate(self._results['Elapsed Time (sec)']):
                if etime > duration:
                    cutoff_index = count
                    break
            if cutoff_index > 0:
                for measure in self._results:
                    cut_results[measure] = self._results[measure][:cutoff_index]

        # Get the cumulative power used in mWh
        cumulative_mwh = {}
        for measure in cut_results:
            if 'cumulative' in measure.lower() and 'mwh' in measure.lower():
                cumulative_mwh[replace_measure_name(measure)] = float(
                    cut_results[measure][-1]
                )

        # Get the power usage rate in Watts
        watt_usage = {}
        for measure in cut_results:
            if 'watt' in measure.lower() and 'limit' not in measure.lower():
                watt_usage[replace_measure_name(measure) + '-avg'] = sum([
                    float(val)
                    for val in cut_results[measure]
                ])/len(cut_results[measure])
                watt_usage[replace_measure_name(measure) + '-max'] = max([
                    float(val)
                    for val in cut_results[measure]
                ])

        # Get average CPU and GPU utilization
        average_utilization = {}
        for utilization in ('CPU Utilization(%)', 'GT Utilization(%)'):
            if utilization not in cut_results:
                self._logger.warning("Could not find measurements for: %s" % utilization)
                continue

            utilized_name = utilization.lower()
            if 'cpu ' in utilized_name:
                utilized_name = 'cpu'
            elif 'gt ' in utilized_name:
                utilized_name = 'gpu'

            average_utilization[utilized_name] = sum([
                float(val)
                for val in cut_results[utilization]
            ])/len(cut_results[utilization])

        # Get average and maximum CPU and GPU frequency
        frequency_info = {'cpu': {}, 'gpu': {}}
        for frequency_measure in ('CPU Frequency_0(MHz)', 'GT Frequency(MHz)'):
            if frequency_measure not in cut_results:
                self._logger.warning("Could not find measurements for: %s" % frequency_measure)
                continue

            fmeasure_name = frequency_measure.lower()
            if 'cpu ' in fmeasure_name:
                fmeasure_name = 'cpu'
            elif 'gt ' in fmeasure_name:
                fmeasure_name = 'gpu'

            frequency_info[fmeasure_name]['favg'] = sum([
                float(val)
                for val in cut_results[frequency_measure]
            ])/len(cut_results[frequency_measure])

            frequency_info[fmeasure_name]['fmax'] = max([
                float(val)
                for val in cut_results[frequency_measure]
            ])

            frequency_info[fmeasure_name]['fmin'] = min([
                float(val)
                for val in cut_results[frequency_measure]
            ])

        summarized_results = {
            "utilization": {
                "type": "power",
                "test": str(test_name) + '-utilization',
                "unit": "%",
                "values": average_utilization
            },
            "power-usage": {
                "type": "power",
                "test": str(test_name) + '-cumulative',
                "unit": "mWh",
                "values": cumulative_mwh
            },
            "power-watts": {
                "type": "power",
                "test": str(test_name) + '-watts',
                "unit": "W",
                "values": watt_usage
            },
            "frequency-cpu": {
                "type": "power",
                "test": str(test_name) + '-frequency-cpu',
                "unit": "MHz",
                "values": frequency_info['cpu']
            },
            "frequency-gpu": {
                "type": "power",
                "test": str(test_name) + '-frequency-gpu',
                "unit": "MHz",
                "values": frequency_info['gpu']
            }
        }

        self._summarized_results = summarized_results
        return self._summarized_results
