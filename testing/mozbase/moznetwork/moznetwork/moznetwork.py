# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import array
import re
import socket
import struct
import subprocess
import sys

import mozinfo
import mozlog
import six

if mozinfo.isLinux:
    import fcntl
if mozinfo.isWin:
    import os


class NetworkError(Exception):
    """Exception thrown when unable to obtain interface or IP."""


def _get_logger():
    logger = mozlog.get_default_logger(component="moznetwork")
    if not logger:
        logger = mozlog.unstructured.getLogger("moznetwork")
    return logger


def _get_interface_list():
    """Provides a list of available network interfaces
    as a list of tuples (name, ip)"""
    logger = _get_logger()
    logger.debug("Gathering interface list")
    max_iface = 32  # Maximum number of interfaces(arbitrary)
    bytes = max_iface * 32
    is_32bit = (8 * struct.calcsize("P")) == 32  # Set Architecture
    struct_size = 32 if is_32bit else 40

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        names = array.array("B", b"\0" * bytes)
        outbytes = struct.unpack(
            "iL",
            fcntl.ioctl(
                s.fileno(),
                0x8912,  # SIOCGIFCONF
                struct.pack("iL", bytes, names.buffer_info()[0]),
            ),
        )[0]
        if six.PY3:
            namestr = names.tobytes()
        else:
            namestr = names.tostring()
        return [
            (
                six.ensure_str(namestr[i : i + 32].split(b"\0", 1)[0]),
                socket.inet_ntoa(namestr[i + 20 : i + 24]),
            )
            for i in range(0, outbytes, struct_size)
        ]

    except IOError:
        raise NetworkError("Unable to call ioctl with SIOCGIFCONF")


def _proc_matches(args, regex):
    """Helper returns the matches of regex in the output of a process created with
    the given arguments"""
    output = subprocess.Popen(
        args=args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    ).stdout.read()
    return re.findall(regex, output)


def _parse_ifconfig():
    """Parse the output of running ifconfig on mac in cases other methods
    have failed"""
    logger = _get_logger()
    logger.debug("Parsing ifconfig")

    # Attempt to determine the default interface in use.
    default_iface = _proc_matches(
        ["route", "-n", "get", "default"], r"interface: (\w+)"
    )

    if default_iface:
        addr_list = _proc_matches(
            ["ifconfig", default_iface[0]], r"inet (\d+.\d+.\d+.\d+)"
        )
        if addr_list:
            logger.debug(
                "Default interface: [%s] %s" % (default_iface[0], addr_list[0])
            )
            if not addr_list[0].startswith("127."):
                return addr_list[0]

    # Iterate over plausible interfaces if we didn't find a suitable default.
    for iface in ["en%s" % i for i in range(10)]:
        addr_list = _proc_matches(["ifconfig", iface], r"inet (\d+.\d+.\d+.\d+)")
        if addr_list:
            logger.debug("Interface: [%s] %s" % (iface, addr_list[0]))
            if not addr_list[0].startswith("127."):
                return addr_list[0]

    # Just return any that isn't localhost. If we can't find one, we have
    # failed.
    addrs = _proc_matches(["ifconfig"], r"inet (\d+.\d+.\d+.\d+)")
    try:
        return [addr for addr in addrs if not addr.startswith("127.")][0]
    except IndexError:
        return None


def _parse_powershell():
    logger = _get_logger()
    logger.debug("Parsing Get-NetIPAdress output via PowerShell")

    try:
        cmd = os.path.join(
            os.environ.get("SystemRoot", "C:\\WINDOWS"),
            "system32",
            "windowspowershell",
            "v1.0",
            "powershell.exe",
        )
        output = subprocess.check_output(
            [
                cmd,
                "(Get-NetIPAddress -AddressFamily IPv4 -AddressState Preferred | Format-List -Property IPAddress)",
            ]
        ).decode("ascii")
        ips = re.findall(r"IPAddress : (\d+.\d+.\d+.\d+)", output)
        for ip in ips:
            logger.debug("IPAddress: %s" % ip)
            if not ip.startswith("127."):
                return ip
        return None
    except FileNotFoundError:
        return None


def get_ip():
    """Provides an available network interface address, for example
    "192.168.1.3".

    A `NetworkError` exception is raised in case of failure."""
    logger = _get_logger()
    try:
        hostname = socket.gethostname()
        try:
            logger.debug("Retrieving IP for %s" % hostname)
            ips = socket.gethostbyname_ex(hostname)[2]
        except socket.gaierror:  # for Mac OS X
            hostname += ".local"
            logger.debug("Retrieving IP for %s" % hostname)
            ips = socket.gethostbyname_ex(hostname)[2]
        if len(ips) == 1:
            ip = ips[0]
        elif len(ips) > 1:
            logger.debug("Multiple addresses found: %s" % ips)
            ip = None
        else:
            ip = None
    except socket.gaierror:
        # sometimes the hostname doesn't resolve to an ip address, in which
        # case this will always fail
        ip = None

    if ip is None or ip.startswith("127."):
        if mozinfo.isLinux:
            interfaces = _get_interface_list()
            for ifconfig in interfaces:
                logger.debug("Interface: [%s] %s" % (ifconfig[0], ifconfig[1]))
                if ifconfig[0] == "lo":
                    continue
                else:
                    return ifconfig[1]
        elif mozinfo.isMac:
            ip = _parse_ifconfig()
        elif mozinfo.isWin:
            ip = _parse_powershell()

    if ip is None:
        raise NetworkError("Unable to obtain network address")

    return ip


def get_lan_ip():
    """Deprecated. Please use get_ip() instead."""
    return get_ip()


def cli(args=sys.argv[1:]):
    parser = argparse.ArgumentParser(description="Retrieve IP address")
    mozlog.commandline.add_logging_group(
        parser, include_formatters=mozlog.commandline.TEXT_FORMATTERS
    )

    args = parser.parse_args()
    mozlog.commandline.setup_logging("mozversion", args, {"mach": sys.stdout})

    _get_logger().info("IP address: %s" % get_ip())


if __name__ == "__main__":
    cli()
