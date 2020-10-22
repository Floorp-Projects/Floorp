#!/usr/bin/env python
"""
Unit-Tests for moznetwork
"""

from __future__ import absolute_import

import re
import subprocess
from distutils.spawn import find_executable

import moznetwork
import pytest

import mock
import mozinfo
import mozunit


@pytest.fixture(scope='session')
def ip_addresses():
    """List of IP addresses associated with the host."""

    # Regex to match IPv4 addresses.
    # 0-255.0-255.0-255.0-255, note order is important here.
    regexip = re.compile("((25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)\.){3}"
                         "(25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)")

    commands = (
        ['ip', 'addr', 'show'],
        ['ifconfig'],
        ['ipconfig'],
        # Explicitly search '/sbin' because it doesn't always appear
        # to be on the $PATH of all systems
        ['/sbin/ip', 'addr', 'show'],
        ['/sbin/ifconfig'],
    )

    cmd = None
    for command in commands:
        if find_executable(command[0]):
            cmd = command
            break
    else:
        raise OSError("No program for detecting ip address found! Ensure one of 'ip', "
                      "'ifconfig' or 'ipconfig' exists on your $PATH.")

    ps = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    standardoutput, _ = ps.communicate()

    # Generate a list of IPs by parsing the output of ip/ifconfig
    return [x.group() for x in re.finditer(regexip, standardoutput.decode('UTF-8'))]


def test_get_ip(ip_addresses):
    """ Attempt to test the IP address returned by
    moznetwork.get_ip() is valid """
    assert moznetwork.get_ip() in ip_addresses


@pytest.mark.skipif(mozinfo.isWin, reason="Test is not supported in Windows")
def test_get_ip_using_get_interface(ip_addresses):
    """ Test that the control flow path for get_ip() using
    _get_interface_list() is works """
    with mock.patch('socket.gethostbyname') as byname:
        # Force socket.gethostbyname to return None
        byname.return_value = None
        assert moznetwork.get_ip() in ip_addresses


if __name__ == '__main__':
    mozunit.main()
