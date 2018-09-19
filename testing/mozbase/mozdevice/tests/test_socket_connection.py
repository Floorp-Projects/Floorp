#!/usr/bin/env python

from __future__ import absolute_import

import mozunit
import pytest

from conftest import random_tcp_port


@pytest.fixture(params=["tcp:{}".format(random_tcp_port()) for _ in range(5)])
def select_test_port(request):
    """Generate a list of ports to be used for testing."""
    yield request.param


def test_list_socket_connections_reverse(mock_adb_object):
    assert [("['reverse',", "'--list']")
            ] == mock_adb_object.list_socket_connections('reverse')


def test_list_socket_connections_forward(mock_adb_object):
    assert [("['forward',", "'--list']")
            ] == mock_adb_object.list_socket_connections('forward')


def test_create_socket_connection_reverse(mock_adb_object,
                                          select_test_port,
                                          redirect_stdout_and_assert):
    _expected = "['reverse', '{0}', '{0}']".format(select_test_port)
    redirect_stdout_and_assert(mock_adb_object.create_socket_connection,
                               direction='reverse',
                               local=select_test_port,
                               remote=select_test_port,
                               text=_expected)


def test_create_socket_connection_forward(mock_adb_object,
                                          select_test_port,
                                          redirect_stdout_and_assert):
    _expected = "['forward', '{0}', '{0}']".format(select_test_port)
    redirect_stdout_and_assert(mock_adb_object.create_socket_connection,
                               direction='forward',
                               local=select_test_port,
                               remote=select_test_port,
                               text=_expected)


def test_remove_socket_connections_reverse(mock_adb_object, redirect_stdout_and_assert):
    _expected = "['reverse', '--remove-all']"
    redirect_stdout_and_assert(mock_adb_object.remove_socket_connections,
                               direction='reverse', text=_expected)


def test_remove_socket_connections_forward(mock_adb_object, redirect_stdout_and_assert):
    _expected = "['forward', '--remove-all']"
    redirect_stdout_and_assert(mock_adb_object.remove_socket_connections,
                               direction='forward', text=_expected)


def test_legacy_forward(mock_adb_object, select_test_port, redirect_stdout_and_assert):
    _expected = "['forward', '{0}', '{0}']".format(select_test_port)
    redirect_stdout_and_assert(mock_adb_object.forward,
                               local=select_test_port,
                               remote=select_test_port,
                               text=_expected)


def test_legacy_reverse(mock_adb_object, select_test_port, redirect_stdout_and_assert):
    _expected = "['reverse', '{0}', '{0}']".format(select_test_port)
    redirect_stdout_and_assert(mock_adb_object.reverse,
                               local=select_test_port,
                               remote=select_test_port,
                               text=_expected)


def test_validate_port_invalid_prefix(mock_adb_object):
    with pytest.raises(ValueError):
        mock_adb_object._validate_port('{}'.format('invalid'), is_local=True)


@pytest.mark.xfail
def test_validate_port_non_numerical_port_identifier(mock_adb_object):
    with pytest.raises(AttributeError):
        mock_adb_object._validate_port('{}'.format(
            'tcp:this:is:not:a:number'), is_local=True)


def test_validate_port_identifier_length_short(mock_adb_object):
    with pytest.raises(ValueError):
        mock_adb_object._validate_port('{}'.format('tcp'), is_local=True)


def test_validate_direction(mock_adb_object):
    with pytest.raises(ValueError):
        mock_adb_object._validate_direction('{}'.format('bad direction'))


if __name__ == '__main__':
    mozunit.main()
