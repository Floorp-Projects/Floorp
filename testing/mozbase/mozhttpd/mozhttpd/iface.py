# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import socket
if os.name != 'nt':
    import fcntl
    import struct

def _get_interface_ip(ifname):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return socket.inet_ntoa(fcntl.ioctl(
            s.fileno(),
            0x8915,  # SIOCGIFADDR
            struct.pack('256s', ifname[:15])
            )[20:24])

def get_lan_ip():
    try:
        ip = socket.gethostbyname(socket.gethostname())
    except socket.gaierror:  # for Mac OS X
        ip = socket.gethostbyname(socket.gethostname() + ".local")

    if ip.startswith("127.") and os.name != "nt":
        interfaces = ["eth0", "eth1", "eth2", "wlan0", "wlan1", "wifi0", "ath0", "ath1", "ppp0"]
        for ifname in interfaces:
            try:
                ip = _get_interface_ip(ifname)
                break;
            except IOError:
                pass
    return ip
