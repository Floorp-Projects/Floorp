# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozunit
import pytest

from marionette_harness.runtests import MarionetteArguments, MarionetteTestRunner


@pytest.mark.parametrize("socket_timeout", ['A', '10', '1B-', '1C2', '44.35'])
def test_parse_arg_socket_timeout(socket_timeout):
    argv = ['marionette', '--socket-timeout', socket_timeout]
    parser = MarionetteArguments()

    def _is_float_convertible(value):
        try:
            float(value)
            return True
        except:
            return False

    if not _is_float_convertible(socket_timeout):
        with pytest.raises(SystemExit) as ex:
            parser.parse_args(args=argv)
        assert ex.value.code == 2
    else:
        args = parser.parse_args(args=argv)
        assert hasattr(args, 'socket_timeout') and args.socket_timeout == float(socket_timeout)


@pytest.mark.parametrize("arg_name, arg_dest, arg_value, expected_value",
                         [('app-arg', 'app_args', 'samplevalue', ['samplevalue']),
                          ('symbols-path', 'symbols_path', 'samplevalue', 'samplevalue'),
                          ('gecko-log', 'gecko_log', 'samplevalue', 'samplevalue'),
                          ('app', 'app', 'samplevalue', 'samplevalue')])
def test_parsing_optional_arguments(mach_parsed_kwargs, arg_name, arg_dest, arg_value,
                                    expected_value):
    parser = MarionetteArguments()
    parsed_args = parser.parse_args(['--' + arg_name, arg_value])
    result = vars(parsed_args)
    assert result.get(arg_dest) == expected_value
    mach_parsed_kwargs[arg_dest] = result[arg_dest]
    runner = MarionetteTestRunner(**mach_parsed_kwargs)
    built_kwargs = runner._build_kwargs()
    assert built_kwargs[arg_dest] == expected_value


@pytest.mark.parametrize("arg_name, arg_dest, arg_value, expected_value",
                         [('adb', 'adb_path', 'samplevalue', 'samplevalue'),
                          ('avd', 'avd', 'samplevalue', 'samplevalue'),
                          ('avd-home', 'avd_home', 'samplevalue', 'samplevalue'),
                          ('package', 'package_name', 'samplevalue', 'samplevalue')])
def test_parse_opt_args_emulator(mach_parsed_kwargs, arg_name, arg_dest, arg_value, expected_value):
    parser = MarionetteArguments()
    parsed_args = parser.parse_args(['--' + arg_name, arg_value])
    result = vars(parsed_args)
    assert result.get(arg_dest) == expected_value
    mach_parsed_kwargs[arg_dest] = result[arg_dest]
    mach_parsed_kwargs["emulator"] = True
    runner = MarionetteTestRunner(**mach_parsed_kwargs)
    built_kwargs = runner._build_kwargs()
    assert built_kwargs[arg_dest] == expected_value


if __name__ == '__main__':
    mozunit.main('--log-tbpl=-')
