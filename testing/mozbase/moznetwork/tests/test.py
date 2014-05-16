#!/usr/bin/env python
"""
Unit-Tests for moznetwork
"""

import os
import mock
import mozinfo
import moznetwork
import re
import subprocess
import unittest


def verify_ip_in_list(ip):
    """
    Helper Method to check if `ip` is listed in Network Adresses
    returned by ipconfig/ifconfig, depending on the platform in use

    :param ip: IPv4 address in the xxx.xxx.xxx.xxx format as a string
                Example Usage:
                    verify_ip_in_list('192.168.0.1')

    returns True if the `ip` is in the list of IPs in ipconfig/ifconfig
    """

    # Regex to match IPv4 addresses.
    # 0-255.0-255.0-255.0-255, note order is important here.
    regexip = re.compile("((25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)\.){3}"
                              "(25[0-5]|2[0-4]\d|1\d\d|[1-9]\d|\d)")

    if mozinfo.isLinux or mozinfo.isMac or mozinfo.isBsd:
        # if "/sbin/ifconfig" exist, use it because it may not be in the
        # PATH (at least on some linux platforms)
        if os.path.isfile('/sbin/ifconfig') and os.access('/sbin/ifconfig',
                                                           os.X_OK):
            args = ['/sbin/ifconfig']
        else:
            args = ["ifconfig"]

    if mozinfo.isWin:
        args = ["ipconfig"]

    ps = subprocess.Popen(args, stdout=subprocess.PIPE)
    standardoutput, standarderror = ps.communicate()

    # Generate a list of IPs by parsing the output of ip/ifconfig
    ip_list = [x.group() for x in re.finditer(regexip, standardoutput)]

    # Check if ip is in list
    if ip in ip_list:
        return True
    else:
        return False


class TestGetIP(unittest.TestCase):

    def test_get_ip(self):
        """ Attempt to test the IP address returned by
        moznetwork.get_ip() is valid """

        ip = moznetwork.get_ip()

        # Check the IP returned by moznetwork is in the list
        self.assertTrue(verify_ip_in_list(ip))

    def test_get_ip_using_get_interface(self):
        """ Test that the control flow path for get_ip() using
        _get_interface_list() is works """

        if mozinfo.isLinux or mozinfo.isMac:

            with mock.patch('socket.gethostbyname') as byname:
                # Force socket.gethostbyname to return None
                byname.return_value = None

                ip = moznetwork.get_ip()

                # Check the IP returned by moznetwork is in the list
                self.assertTrue(verify_ip_in_list(ip))


if __name__ == '__main__':
    unittest.main()
