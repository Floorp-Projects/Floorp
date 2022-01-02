#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import fnmatch
import platform

def main():
    host_arch = platform.machine().lower()
    if host_arch in ('amd64', 'x86_64'):
        host_arch = 'x64'
    elif fnmatch.fnmatch(host_arch, 'i?86') or host_arch == 'i86pc':
        host_arch = 'ia32'
    elif host_arch == 'arm64':
        pass
    elif host_arch.startswith('arm'):
        host_arch = 'arm'
    elif host_arch.startswith('mips64'):
        host_arch = 'mips64'
    elif host_arch.startswith('mips'):
        host_arch = 'mips'
    print(host_arch)

if __name__ == '__main__':
    main()
