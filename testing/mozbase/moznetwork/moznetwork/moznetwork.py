# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import socket
import array
import re
import struct
import subprocess
import mozinfo

if mozinfo.isLinux:
    import fcntl

class NetworkError(Exception):
    """Exception thrown when unable to obtain interface or IP."""


def _get_interface_list():
    """Provides a list of available network interfaces
       as a list of tuples (name, ip)"""
    max_iface = 32  # Maximum number of interfaces(Aribtrary)
    bytes = max_iface * 32
    is_32bit = (8 * struct.calcsize("P")) == 32  # Set Architecture
    struct_size = 32 if is_32bit else 40

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        names = array.array('B', '\0' * bytes)
        outbytes = struct.unpack('iL', fcntl.ioctl(
            s.fileno(),
            0x8912,  # SIOCGIFCONF
            struct.pack('iL', bytes, names.buffer_info()[0])
        ))[0]
        namestr = names.tostring()
        return [(namestr[i:i + 32].split('\0', 1)[0],
                socket.inet_ntoa(namestr[i + 20:i + 24]))\
                for i in range(0, outbytes, struct_size)]

    except IOError:
        raise NetworkError('Unable to call ioctl with SIOCGIFCONF')

def _proc_matches(args, regex):
    """Helper returns the matches of regex in the output of a process created with
    the given arguments"""
    output = subprocess.Popen(args=args,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT).stdout.read()
    return re.findall(regex, output)

def _parse_ifconfig():
    """Parse the output of running ifconfig on mac in cases other methods
    have failed"""

    # Attempt to determine the default interface in use.
    default_iface = _proc_matches(['route', '-n', 'get', 'default'],
                                  'interface: (\w+)')
    if default_iface:
        addr_list = _proc_matches(['ifconfig', default_iface[0]],
                                  'inet (\d+.\d+.\d+.\d+)')
        if addr_list and not addr_list[0].startswith('127.'):
            return addr_list[0]

    # Iterate over plausible interfaces if we didn't find a suitable default.
    for iface in ['en%s' % i for i in range(10)]:
        addr_list = _proc_matches(['ifconfig', iface],
                                  'inet (\d+.\d+.\d+.\d+)')
        if addr_list and not addr_list[0].startswith('127.'):
            return addr_list[0]

    # Just return any that isn't localhost. If we can't find one, we have
    # failed.
    addrs = _proc_matches(['ifconfig'],
                          'inet (\d+.\d+.\d+.\d+)')
    try:
        return [addr for addr in addrs if not addr.startswith('127.')][0]
    except IndexError:
        return None

def get_ip():
    """Provides an available network interface address, for example
       "192.168.1.3".

       A `NetworkError` exception is raised in case of failure."""
    try:
        try:
            ip = socket.gethostbyname(socket.gethostname())
        except socket.gaierror:  # for Mac OS X
            ip = socket.gethostbyname(socket.gethostname() + ".local")
    except socket.gaierror:
        # sometimes the hostname doesn't resolve to an ip address, in which
        # case this will always fail
        ip = None

    if ip is None or ip.startswith("127."):
        if mozinfo.isLinux:
            interfaces = _get_interface_list()
            for ifconfig in interfaces:
                if ifconfig[0] == 'lo':
                    continue
                else:
                    return ifconfig[1]
        elif mozinfo.isMac:
            ip = _parse_ifconfig()

    if ip is None:
        raise NetworkError('Unable to obtain network address')

    return ip


def get_lan_ip():
    """Deprecated. Please use get_ip() instead."""
    return get_ip()
