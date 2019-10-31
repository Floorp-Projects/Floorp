#!/usr/bin/env python

from __future__ import absolute_import

import mock
import mozunit
import pytest
import subprocess
import sys

from mozpower import MozPower
from mozpower.mozpower import (
    OsCpuComboMissingError,
    MissingProcessorInfoError,
)


def test_mozpower_android_init_failure():
    """Tests that the MozPower object fails when the android
    flag is set. Remove this test once android is implemented.
    """
    with pytest.raises(NotImplementedError):
        MozPower(android=True)


def test_mozpower_oscpu_combo_missing_error():
    """Tests that the error OsCpuComboMissingError is raised
    when we can't find a OS, and CPU combination (and, therefore, cannot
    find a power measurer).
    """
    with mock.patch.object(MozPower, '_get_os', return_value='Not-An-OS') as _, \
            mock.patch.object(
                MozPower, '_get_processor_info', return_value='Not-A-Processor'
            ) as _:
        with pytest.raises(OsCpuComboMissingError):
            MozPower()


def test_mozpower_processor_info_missing_error():
    """Tests that the error MissingProcessorInfoError is raised
    when failures occur during processor information parsing.
    """
    # The builtins module name differs between python 2 and 3
    builtins_name = '__builtin__'
    if sys.version_info[0] == 3:
        builtins_name = 'builtins'

    def os_side_effect_true(*args, **kwargs):
        """Used as a passing side effect for os.path.exists calls.
        """
        return True

    def os_side_effect_false(*args, **kwargs):
        """Used as a failing side effect for os.path.exists calls.
        """
        return False

    def subprocess_side_effect_fail(*args, **kwargs):
        """Used to mock a failure in subprocess.check_output calls.
        """
        raise subprocess.CalledProcessError(1, "Testing failure")

    # Test failures in macos processor information parsing
    with mock.patch.object(
            MozPower, '_get_os', return_value='Darwin'
         ) as _:

        with mock.patch('os.path.exists') as os_mock:
            os_mock.side_effect = os_side_effect_false

            # Check that we fail properly if the processor
            # information file doesn't exist.
            with pytest.raises(MissingProcessorInfoError):
                MozPower()

            # Check that we fail properly when an error occurs
            # in the subprocess call.
            os_mock.side_effect = os_side_effect_true
            with mock.patch('subprocess.check_output') as subprocess_mock:
                subprocess_mock.side_effect = subprocess_side_effect_fail
                with pytest.raises(MissingProcessorInfoError):
                    MozPower()

    # Test failures in linux processor information parsing
    with mock.patch.object(
        MozPower, '_get_os', return_value='Linux'
     ) as _:

        with mock.patch('os.path.exists') as os_mock:
            os_mock.side_effect = os_side_effect_false

            # Check that we fail properly if the processor
            # information file doesn't exist.
            with pytest.raises(MissingProcessorInfoError):
                MozPower()

            # Check that we fail properly when the model cannot be found
            # with by searching for 'model name'.
            os_mock.side_effect = os_side_effect_true
            with mock.patch(
                    '%s.open' % builtins_name, mock.mock_open(read_data="")
                 ) as _:
                with pytest.raises(MissingProcessorInfoError):
                    MozPower()


def test_mozpower_oscpu_combo(mozpower_obj):
    """Tests that the correct class is instantiated for a given
    OS and CPU combination (MacIntelPower in this case).
    """
    assert mozpower_obj.measurer.__class__.__name__ == 'MacIntelPower'
    assert mozpower_obj.measurer._os == 'darwin' and mozpower_obj.measurer._cpu == 'intel'


def test_mozpower_measuring(mozpower_obj):
    """Tests that measurers are properly called with each method.
    """
    with mock.patch(
                'mozpower.macintelpower.MacIntelPower.initialize_power_measurements'
            ) as _, \
            mock.patch(
                'mozpower.macintelpower.MacIntelPower.finalize_power_measurements'
            ) as _, \
            mock.patch(
                'mozpower.macintelpower.MacIntelPower.get_perfherder_data'
            ) as _:
        mozpower_obj.initialize_power_measurements()
        mozpower_obj.measurer.initialize_power_measurements.assert_called()

        mozpower_obj.finalize_power_measurements()
        mozpower_obj.measurer.finalize_power_measurements.assert_called()

        mozpower_obj.get_perfherder_data()
        mozpower_obj.measurer.get_perfherder_data.assert_called()


def test_mozpower_measuring_with_no_measurer(mozpower_obj):
    """Tests that no errors occur when the measurer is None, and the
    initialize, finalize, and get_perfherder_data functions are called.
    """
    with mock.patch(
                'mozpower.macintelpower.MacIntelPower.initialize_power_measurements'
            ) as _, \
            mock.patch(
                'mozpower.macintelpower.MacIntelPower.finalize_power_measurements'
            ) as _, \
            mock.patch(
                'mozpower.macintelpower.MacIntelPower.get_perfherder_data'
            ) as _:
        measurer = mozpower_obj.measurer
        mozpower_obj.measurer = None

        mozpower_obj.initialize_power_measurements()
        assert not measurer.initialize_power_measurements.called

        mozpower_obj.finalize_power_measurements()
        assert not measurer.finalize_power_measurements.called

        mozpower_obj.get_perfherder_data()
        assert not measurer.get_perfherder_data.called

        mozpower_obj.get_full_perfherder_data("mozpower")
        assert not measurer.get_perfherder_data.called


def test_mozpower_get_full_perfherder_data(mozpower_obj):
    """Tests that the full perfherder data blob is properly
    produced given a partial perfherder data blob with correct
    entries.
    """
    partial_perfherder = {
        'utilization': {
            'type': 'power',
            'test': 'mozpower',
            'unit': '%',
            'values': {
                'cpu': 50,
                'gpu': 0
            }
        },
        'power-usage': {
            'type': 'power',
            'test': 'mozpower',
            'unit': 'mWh',
            'values': {
                'cpu': 2.0,
                'dram': 0.1,
                'gpu': 4.0
            }
        },
        'frequency-cpu': {
            'type': 'power',
            'test': 'mozpower',
            'unit': 'MHz',
            'values': {
                'cpu-favg': 2.0,
                'cpu-fmax': 5.0,
                'cpu-fmin': 0.0,
            }
        },
        'frequency-gpu': {
            'type': 'power',
            'test': 'mozpower',
            'unit': 'MHz',
            'values': {
                'gpu-favg': 3.0,
                'gpu-fmax': 6.0,
                'gpu-fmin': 0.0
            }
        }
    }
    utilization_vals = [0, 50]
    power_usage_vals = [2.0, 0.1, 4.0]
    frequency_cpu_vals = [2.0, 5.0, 0.0]
    frequency_gpu_vals = [3.0, 6.0, 0.0]

    with mock.patch(
            'mozpower.macintelpower.MacIntelPower.get_perfherder_data'
         ) as gpd:
        gpd.return_value = partial_perfherder

        full_perfherder = mozpower_obj.get_full_perfherder_data('mozpower')
        assert full_perfherder['framework']['name'] == 'mozpower'
        assert len(full_perfherder['suites']) == 4

        # Check that each of the two suites were created correctly.
        suites = full_perfherder['suites']
        for suite in suites:
            assert 'subtests' in suite

            assert suite['type'] == 'power'
            assert suite['alertThreshold'] == 2.0
            assert suite['lowerIsBetter']

            all_vals = []
            for subtest in suite['subtests']:
                assert 'value' in subtest

                # Check that the subtest names were created correctly
                if 'utilization' in suite['name']:
                    assert 'utilization' in subtest['name']
                elif 'power-usage' in suite['name']:
                    assert 'power-usage' in subtest['name']
                elif 'frequency-cpu' in suite['name']:
                    assert 'frequency-cpu' in subtest['name']
                elif 'frequency-gpu' in suite['name']:
                    assert 'frequency-gpu' in subtest['name']
                else:
                    assert False, "Unknown subtest name %s" % subtest['name']

                all_vals.append(subtest['value'])

            if 'utilization' in suite['name']:
                assert len(all_vals) == 2
                assert suite['unit'] == '%'
                assert suite['name'] == 'mozpower-utilization'
                assert not list(set(all_vals) - set(utilization_vals))
                assert suite['value'] == float(25)
            elif 'power-usage' in suite['name']:
                assert len(all_vals) == 3
                assert suite['unit'] == 'mWh'
                assert suite['name'] == 'mozpower-power-usage'
                assert not list(set(all_vals) - set(power_usage_vals))
                assert suite['value'] == float(6.1)
            elif 'frequency-cpu' in suite['name']:
                assert len(all_vals) == 3
                assert suite['unit'] == 'MHz'
                assert suite['name'] == 'mozpower-frequency-cpu'
                assert not list(set(all_vals) - set(frequency_cpu_vals))
                assert suite['value'] == float(2.0)
            elif 'frequency-gpu' in suite['name']:
                assert len(all_vals) == 3
                assert suite['unit'] == 'MHz'
                assert suite['name'] == 'mozpower-frequency-gpu'
                assert not list(set(all_vals) - set(frequency_gpu_vals))
                assert suite['value'] == float(3.0)
            else:
                assert False, "Unknown suite name %s" % suite['name']


if __name__ == '__main__':
    mozunit.main()
