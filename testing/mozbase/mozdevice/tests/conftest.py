from __future__ import absolute_import, print_function

import sys
from random import randint, seed

import mozdevice
import pytest
from mock import patch
from six import StringIO

# set up required module-level variables/objects
seed(1488590)


def random_tcp_port():
    """Returns a pseudo-random integer generated from a seed.

    :returns: int: pseudo-randomly generated integer
    """
    return randint(8000, 12000)


@pytest.fixture(autouse=True)
def mock_command_output(monkeypatch):
    """Monkeypatches the ADBDevice.command_output() method call.

    Instead of calling the concrete method implemented in adb.py::ADBDevice,
    this method simply returns a string representation of the command that was
    received.

    :param object monkeypatch: pytest provided fixture for mocking.
    """

    def command_output_wrapper(object, cmd, timeout):
        """Actual monkeypatch implementation of the comand_output method call.

        :param object object: placeholder object representing ADBDevice
        :param str cmd: command to be executed
        :param timeout: unused parameter to represent timeout threshold
        :returns: string - string representation of command to be executed
        """
        print(str(cmd))
        return str(cmd)

    monkeypatch.setattr(mozdevice.ADBDevice,
                        'command_output', command_output_wrapper)


@pytest.fixture(autouse=True)
def mock_shell_output(monkeypatch):
    """Monkeypatches the ADBDevice.shell_output() method call.

    Instead of returning the output of an adb call, this method will
    return appropriate string output. Content of the string output is
    in line with the calling method's expectations.

    :param object monkeypatch: pytest provided fixture for mocking.
    """

    def shell_output_wrapper(object, cmd, env=None, cwd=None, timeout=None,
                             enable_run_as=False):
        """Actual monkeypatch implementation of the shell_output method call.

        :param object object: placeholder object representing ADBDevice
        :param str cmd: command to be executed
        :param env: contains the environment variable
        :type env: dict or None
        :param cwd: The directory from which to execute.
        :type cwd: str or None
        :param timeout: unused parameter tp represent timeout threshold
        :param enable_run_as: bool determining if run_as <app> is to be used
        :returns: string - string representation of a simulated call to adb
        """
        if 'pm list package error' in cmd:
            return 'Error: Could not access the Package Manager'
        elif 'pm list package none' in cmd:
            return ''
        elif 'pm list package' in cmd:
            apps = ["org.mozilla.fennec", "org.mozilla.geckoview_example"]
            return ('package:{}\n' * len(apps)).format(*apps)
        else:
            print(str(cmd))
            return str(cmd)

    monkeypatch.setattr(mozdevice.ADBDevice, 'shell_output', shell_output_wrapper)


@pytest.fixture(autouse=True)
def mock_is_path_internal_storage(monkeypatch):
    """Monkeypatches the ADBDevice.is_path_internal_storage() method call.

    Instead of returning the outcome of whether the path provided is
    internal storage or external, this will always return True.

    :param object monkeypatch: pytest provided fixture for mocking.
    """

    def is_path_internal_storage_wrapper(object, path, timeout=None):
        """Actual monkeypatch implementation of the is_path_internal_storage() call.

        :param str path: The path to test.
        :param timeout: The maximum time in
            seconds for any spawned adb process to complete before
            throwing an ADBTimeoutError.  This timeout is per adb call. The
            total time spent may exceed this value. If it is not
            specified, the value set in the ADBDevice constructor is used.
        :returns: boolean

        :raises: * ADBTimeoutError
                 * ADBError
        """
        if 'internal_storage' in path:
            return True
        return False

    monkeypatch.setattr(mozdevice.ADBDevice,
                        'is_path_internal_storage', is_path_internal_storage_wrapper)


@pytest.fixture(autouse=True)
def mock_enable_run_as_for_path(monkeypatch):
    """Monkeypatches the ADBDevice.enable_run_as_for_path(path) method.

    Always return True

    :param object monkeypatch: pytest provided fixture for mocking.
    """

    def enable_run_as_for_path_wrapper(object, path):
        """Actual monkeypatch implementation of the enable_run_as_for_path() call.

        :param str path: The path to test.
        :returns: boolean
        """
        return True

    monkeypatch.setattr(mozdevice.ADBDevice,
                        'enable_run_as_for_path', enable_run_as_for_path_wrapper)


@pytest.fixture(autouse=True)
def mock_shell_bool(monkeypatch):
    """Monkeypatches the ADBDevice.shell_bool() method call.

    Instead of returning the output of an adb call, this method will
    return appropriate string output. Content of the string output is
    in line with the calling method's expectations.

    :param object monkeypatch: pytest provided fixture for mocking.
    """

    def shell_bool_wrapper(object, cmd, env=None, cwd=None, timeout=None,
                           enable_run_as=False):
        """Actual monkeypatch implementation of the shell_bool method call.

        :param object object: placeholder object representing ADBDevice
        :param str cmd: command to be executed
        :param env: contains the environment variable
        :type env: dict or None
        :param cwd: The directory from which to execute.
        :type cwd: str or None
        :param timeout: unused parameter tp represent timeout threshold
        :param enable_run_as: bool determining if run_as <app> is to be used
        :returns: string - string representation of a simulated call to adb
        """
        print(cmd)
        return str(cmd)

    monkeypatch.setattr(mozdevice.ADBDevice, 'shell_bool', shell_bool_wrapper)


@pytest.fixture(autouse=True)
def mock_adb_object():
    """Patches the __init__ method call when instantiating ADBDevice.

    ADBDevice normally requires instantiated objects in order to execute
    its commands.

    With a pytest-mock patch, we are able to mock the initialization of
    the ADBDevice object. By yielding the instantiated mock object,
    unit tests can be run that call methods that require an instantiated
    object.

    :yields: ADBDevice - mock instance of ADBDevice object
    """
    with patch.object(mozdevice.ADBDevice, '__init__', lambda self: None):
        yield mozdevice.ADBDevice()


@pytest.fixture
def redirect_stdout_and_assert():
    """Redirects the stdout pipe temporarily to a StringIO stream.

    This is useful to assert on methods that do not return
    a value, such as most ADBDevice methods.

    The original stdout pipe is preserved throughout the process.

    :returns: _wrapper method
    """

    def _wrapper(func, **kwargs):
        """Implements the stdout sleight-of-hand.

        After preserving the original sys.stdout, it is switched
        to use cStringIO.StringIO.

        Method with no return value is called, and the stdout
        pipe is switched back to the original sys.stdout.

        The expected outcome is received as part of the kwargs.
        This is asserted against a sanitized output from the method
        under test.

        :param object func: method under test
        :param dict kwargs: dictionary of function parameters
        """
        original_stdout = sys.stdout
        sys.stdout = testing_stdout = StringIO()
        expected_text = kwargs.pop('text')
        func(**kwargs)
        sys.stdout = original_stdout
        assert expected_text in testing_stdout.getvalue().rstrip()

    return _wrapper
