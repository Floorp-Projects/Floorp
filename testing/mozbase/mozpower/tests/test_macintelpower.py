#!/usr/bin/env python

from __future__ import absolute_import

import mock
import mozunit
import time


def test_macintelpower_init(macintelpower_obj):
    """Tests that the MacIntelPower object is correctly initialized.
    """
    assert macintelpower_obj.ipg_path
    assert macintelpower_obj.ipg
    assert macintelpower_obj._os == 'darwin'
    assert macintelpower_obj._cpu == 'intel'


def test_macintelpower_measuring(macintelpower_obj):
    """Tests that measurement initialization and finalization works
    for the MacIntelPower object.
    """
    assert not macintelpower_obj.start_time
    assert not macintelpower_obj.ipg._running
    assert not macintelpower_obj.ipg._output_files
    macintelpower_obj.initialize_power_measurements()

    # Check that initialization set start_time, and started the
    # IPG measurer thread.

    # Wait a bit for thread to start, then check it
    timeout = 10
    start = time.time()
    while time.time() - start < timeout and not macintelpower_obj.ipg._running:
        time.sleep(1)

    assert macintelpower_obj.start_time
    assert macintelpower_obj.ipg._running

    test_data = {"power-usage": "data"}

    def formatter_side_effect(*args, **kwargs):
        return test_data

    with mock.patch(
            'mozpower.intel_power_gadget.IPGResultsHandler.clean_ipg_data'
         ) as _:
        with mock.patch(
                'mozpower.intel_power_gadget.IPGResultsHandler.'
                'format_ipg_data_to_partial_perfherder'
             ) as formatter:
            formatter.side_effect = formatter_side_effect

            macintelpower_obj.finalize_power_measurements(wait_interval=2, timeout=30)

            # Check that finalization set the end_time, stopped the IPG measurement
            # thread, added atleast one output file name, and initialized
            # an IPGResultsHandler object
            assert macintelpower_obj.end_time
            assert not macintelpower_obj.ipg._running
            assert macintelpower_obj.ipg._output_files
            assert macintelpower_obj.ipg_results_handler

            # Check that the IPGResultHandler's methods were
            # called
            macintelpower_obj.ipg_results_handler. \
                clean_ipg_data.assert_called()
            macintelpower_obj.ipg_results_handler. \
                format_ipg_data_to_partial_perfherder.assert_called_once_with(
                    macintelpower_obj.end_time - macintelpower_obj.start_time, 'power-testing'
                )

            # Make sure we can get the expected perfherder data
            # after formatting
            assert macintelpower_obj.get_perfherder_data() == test_data


if __name__ == '__main__':
    mozunit.main()
