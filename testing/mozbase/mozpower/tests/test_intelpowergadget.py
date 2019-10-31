#!/usr/bin/env python

from __future__ import absolute_import

import datetime
import mock
import mozunit
import os
import pytest
import time

from mozpower.intel_power_gadget import (
    IPGEmptyFileError,
    IPGMissingOutputFileError,
    IPGTimeoutError,
    IPGUnknownValueTypeError,
)


def test_ipg_pathsplitting(ipg_obj):
    """Tests that the output file path and prefix was properly split.
    This test assumes that it is in the same directory as the conftest.py file.
    """
    assert ipg_obj.output_dir_path == os.path.abspath(os.path.dirname(__file__)) + '/files'
    assert ipg_obj.output_file_prefix == 'raptor-tp6-amazon-firefox_powerlog'


def test_ipg_get_output_file_path(ipg_obj):
    """Tests that the output file path is constantly changing
    based on the file_counter value.
    """
    test_path = '/test_path/'
    test_ext = '.txt'
    ipg_obj._file_counter = 1
    ipg_obj._output_dir_path = test_path
    ipg_obj._output_file_ext = test_ext

    for i in range(1, 6):
        fpath = ipg_obj._get_output_file_path()

        assert fpath.startswith(test_path)
        assert fpath.endswith(test_ext)
        assert str(i) in fpath


def test_ipg_start_and_stop(ipg_obj):
    """Tests that the IPG thread can start and stop properly.
    """
    def subprocess_side_effect(*args, **kwargs):
        time.sleep(1)

    with mock.patch('subprocess.check_output') as m:
        m.side_effect = subprocess_side_effect

        # Start recording IPG measurements
        ipg_obj.start_ipg()
        assert not ipg_obj._stop

        # Wait a bit for thread to start, then check it
        timeout = 10
        start = time.time()
        while time.time() - start < timeout and not ipg_obj._running:
            time.sleep(1)

        assert ipg_obj._running
        assert ipg_obj._thread.isAlive()

        # Stop recording IPG measurements
        ipg_obj.stop_ipg(wait_interval=1, timeout=30)
        assert ipg_obj._stop
        assert not ipg_obj._running


def test_ipg_stopping_timeout(ipg_obj):
    """Tests that an IPGTimeoutError is raised when
    the thread is still "running" and the wait in _wait_for_ipg
    has exceeded the timeout value.
    """
    with pytest.raises(IPGTimeoutError):
        ipg_obj._running = True
        ipg_obj._wait_for_ipg(wait_interval=1, timeout=2)


def test_ipg_rh_combine_cumulatives(ipg_rh_obj):
    """Tests that cumulatives are correctly combined in
    the _combine_cumulative_rows function.
    """
    cumulatives_to_combine = [
        [0, 1, 2, 3, 4, 5],
        [0, 1, 2, 3, 4, 5],
        [0, 1, 2, 3, 4, 5],
        [0, 1, 2, 3, 4, 5]
    ]

    combined_cumulatives = ipg_rh_obj._combine_cumulative_rows(cumulatives_to_combine)

    # Check that accumulation worked, final value must be the maximum
    assert combined_cumulatives[-1] == max(combined_cumulatives)

    # Check that the cumulative values are monotonically increasing
    for count, val in enumerate(combined_cumulatives[:-1]):
        assert combined_cumulatives[count+1] - val >= 0


def test_ipg_rh_clean_file(ipg_rh_obj):
    """Tests that IPGResultsHandler correctly cleans the data
    from one file.
    """
    file = ipg_rh_obj._output_files[0]
    linecount = 0
    with open(file, 'r') as f:
        for line in f:
            linecount += 1

    results, summary, clean_file = ipg_rh_obj._clean_ipg_file(file)

    # Check that each measure from the csv header
    # is in the results dict and that the clean file output
    # exists.
    for measure in results:
        assert measure in ipg_rh_obj._csv_header
    assert os.path.exists(clean_file)

    clean_rows = []
    with open(clean_file, 'r') as f:
        for line in f:
            if line.strip():
                clean_rows.append(line)

    # Make sure that the results and summary entries
    # have the expected lengths.
    for measure in results:
        # Add 6 for new lines that were removed
        assert len(results[measure]) + len(summary) + 6 == linecount
        # Subtract 1 for the csv header
        assert len(results[measure]) == len(clean_rows) - 1


def test_ipg_rh_clean_ipg_data_no_files(ipg_rh_obj):
    """Tests that IPGResultsHandler correctly handles the case
    when no output files exist.
    """
    ipg_rh_obj._output_files = []
    clean_data = ipg_rh_obj.clean_ipg_data()
    assert clean_data is None


def test_ipg_rh_clean_ipg_data(ipg_rh_obj):
    """Tests that IPGResultsHandler correctly handles cleaning
    all known files and that the results and the merged output
    are correct.
    """
    clean_data = ipg_rh_obj.clean_ipg_data()
    clean_files = ipg_rh_obj.cleaned_files
    merged_output_path = ipg_rh_obj.merged_output_path

    # Check that the expected output exists
    assert clean_data is not None
    assert len(clean_files) == len(ipg_rh_obj._output_files)
    assert os.path.exists(merged_output_path)

    # Check that the merged file length and results length
    # is correct, and that no lines were lost and no extra lines
    # were added.
    expected_merged_line_count = 0
    for file in clean_files:
        with open(file, 'r') as f:
            for count, line in enumerate(f):
                if count == 0:
                    continue
                if line.strip():
                    expected_merged_line_count += 1

    merged_line_count = 0
    with open(merged_output_path, 'r') as f:
        for count, line in enumerate(f):
            if count == 0:
                continue
            if line.strip():
                merged_line_count += 1

    assert merged_line_count == expected_merged_line_count
    for measure in clean_data:
        assert len(clean_data[measure]) == merged_line_count

    # Check that the clean data rows are ordered in increasing time
    times_in_seconds = []
    for sys_time in clean_data['System Time']:
        split_sys_time = sys_time.split(':')
        hour_min_sec = ':'.join(split_sys_time[:-1])
        millis = float(split_sys_time[-1])/1000

        timestruct = time.strptime(hour_min_sec, '%H:%M:%S')
        times_in_seconds.append(
            datetime.timedelta(
                hours=timestruct.tm_hour,
                minutes=timestruct.tm_min,
                seconds=timestruct.tm_sec
            ).total_seconds() + millis
        )

    for count, val in enumerate(times_in_seconds[:-1]):
        assert times_in_seconds[count+1] - val >= 0


def test_ipg_rh_format_to_perfherder_with_no_results(ipg_rh_obj):
    """Tests that formatting the data to a perfherder-like format
    fails when clean_ipg_data was not called beforehand.
    """
    formatted_data = ipg_rh_obj.format_ipg_data_to_partial_perfherder(
        1000, ipg_rh_obj._output_file_prefix
    )
    assert formatted_data is None


def test_ipg_rh_format_to_perfherder_without_cutoff(ipg_rh_obj):
    """Tests that formatting the data to a perfherder-like format
    works as expected.
    """
    ipg_rh_obj.clean_ipg_data()
    formatted_data = ipg_rh_obj.format_ipg_data_to_partial_perfherder(
        1000, ipg_rh_obj._output_file_prefix
    )

    # Check that the expected entries exist
    assert len(formatted_data.keys()) == 5
    assert 'utilization' in formatted_data and 'power-usage' in formatted_data

    assert formatted_data['power-usage']['test'] == \
        ipg_rh_obj._output_file_prefix + '-cumulative'
    assert formatted_data['utilization']['test'] == \
        ipg_rh_obj._output_file_prefix + '-utilization'
    assert formatted_data['frequency-gpu']['test'] == \
        ipg_rh_obj._output_file_prefix + '-frequency-gpu'
    assert formatted_data['frequency-cpu']['test'] == \
        ipg_rh_obj._output_file_prefix + '-frequency-cpu'
    assert formatted_data['power-watts']['test'] == \
        ipg_rh_obj._output_file_prefix + '-watts'

    for measure in formatted_data:
        # Make sure that the data exists
        assert len(formatted_data[measure]['values']) >= 1

        for valkey in formatted_data[measure]['values']:
            # Make sure the names were simplified
            assert '(' not in valkey
            assert ')' not in valkey

    # Check that gpu utilization doesn't exist but cpu does
    utilization_vals = formatted_data['utilization']['values']
    assert 'cpu' in utilization_vals
    assert 'gpu' not in utilization_vals

    expected_fields = ['processor-cores', 'processor-package', 'gpu', 'dram']
    consumption_vals = formatted_data['power-usage']['values']

    consumption_vals_measures = list(consumption_vals.keys())

    # This assertion ensures that the consumption values contain the expected
    # fields and nothing more.
    assert not list(set(consumption_vals_measures) - set(expected_fields))


def test_ipg_rh_format_to_perfherder_with_cutoff(ipg_rh_obj):
    """Tests that formatting the data to a perfherder-like format
    works as expected.
    """
    ipg_rh_obj.clean_ipg_data()
    formatted_data = ipg_rh_obj.format_ipg_data_to_partial_perfherder(
        2.5, ipg_rh_obj._output_file_prefix
    )

    # Check that the formatted data was cutoff at the correct point,
    # expecting that only the first row of merged will exist.
    utilization_vals = formatted_data['utilization']['values']
    assert utilization_vals['cpu'] == 14

    # Expected vals are ordered in this way: [processor, cores, dram, gpu]
    expected_vals = [6.517, 5.847, 0.244, 0.006]
    consumption_vals = [
        formatted_data['power-usage']['values'][measure]
        for measure in formatted_data['power-usage']['values']
    ]
    assert not list(set(expected_vals) - set(consumption_vals))


def test_ipg_rh_missingoutputfile(ipg_rh_obj):
    """Tests that the IPGMissingOutputFileError is raised
    when a bad file path is passed to _clean_ipg_file.
    """
    bad_files = ['non-existent-file']
    with pytest.raises(IPGMissingOutputFileError):
        ipg_rh_obj._clean_ipg_file(bad_files[0])

    ipg_rh_obj._output_files = bad_files
    with pytest.raises(IPGMissingOutputFileError):
        ipg_rh_obj.clean_ipg_data()


def test_ipg_rh_emptyfile(ipg_rh_obj):
    """Tests that the empty file error is raised when
    a file exists, but does not contain any results in
    it.
    """
    base_path = os.path.abspath(os.path.dirname(__file__)) + '/files/'
    bad_files = [base_path + 'emptyfile.txt']
    with pytest.raises(IPGEmptyFileError):
        ipg_rh_obj._clean_ipg_file(bad_files[0])

    ipg_rh_obj._output_files = bad_files
    with pytest.raises(IPGEmptyFileError):
        ipg_rh_obj.clean_ipg_data()


def test_ipg_rh_valuetypeerrorfile(ipg_rh_obj):
    """Tests that the IPGUnknownValueTypeError is raised
    when a bad entry is encountered in a file that is cleaned.
    """
    base_path = os.path.abspath(os.path.dirname(__file__)) + '/files/'
    bad_files = [base_path + 'valueerrorfile.txt']
    with pytest.raises(IPGUnknownValueTypeError):
        ipg_rh_obj._clean_ipg_file(bad_files[0])

    ipg_rh_obj._output_files = bad_files
    with pytest.raises(IPGUnknownValueTypeError):
        ipg_rh_obj.clean_ipg_data()


if __name__ == '__main__':
    mozunit.main()
