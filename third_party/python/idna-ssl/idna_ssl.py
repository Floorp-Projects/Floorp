import ssl
import sys

import idna

__version__ = '1.1.0'

real_match_hostname = ssl.match_hostname

PY_370 = sys.version_info >= (3, 7, 0)


def patched_match_hostname(cert, hostname):
    try:
        hostname = idna.encode(hostname, uts46=True).decode('ascii')
    except UnicodeError:
        hostname = hostname.encode('idna').decode('ascii')

    return real_match_hostname(cert, hostname)


def patch_match_hostname():
    if PY_370:
        return

    if hasattr(ssl.match_hostname, 'patched'):
        return

    ssl.match_hostname = patched_match_hostname
    ssl.match_hostname.patched = True


def reset_match_hostname():
    if PY_370:
        return

    if not hasattr(ssl.match_hostname, 'patched'):
        return

    ssl.match_hostname = real_match_hostname
