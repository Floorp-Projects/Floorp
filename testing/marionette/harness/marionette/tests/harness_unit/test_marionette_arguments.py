# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pytest

from marionette.runtests import MarionetteArguments


@pytest.mark.parametrize("sock_timeout_value", ['A', '10', '1B-', '1C2', '44.35'])
def test_parse_arg_socket_timeout_with_multiple_values(sock_timeout_value):
    argv = ['marionette', '--socket-timeout', sock_timeout_value]
    parser = MarionetteArguments()

    def _is_float_convertible(value):
        try:
            float(value)
            return True
        except:
            return False

    if not _is_float_convertible(sock_timeout_value):
        # should raising exception, since sock_timeout must be convertible to float.
        with pytest.raises(SystemExit) as ex:
            parser.parse_args(args=argv)
        assert ex.value.code == 2
    else:
        # should pass without raising exception.
        args = parser.parse_args(args=argv)
        assert hasattr(args, 'socket_timeout') and args.socket_timeout == float(sock_timeout_value)


if __name__ == '__main__':
    import sys
    sys.exit(pytest.main(['--verbose', __file__]))
