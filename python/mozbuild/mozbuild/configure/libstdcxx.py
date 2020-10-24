#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# This script find the version of libstdc++ and prints it as single number
# with 8 bits per element. For example, GLIBCXX_3.4.10 becomes
# 3 << 16 | 4 << 8 | 10 = 197642. This format is easy to use
# in the C preprocessor.

# We find out both the host and target versions. Since the output
# will be used from shell, we just print the two assignments and evaluate
# them from shell.

from __future__ import absolute_import, print_function

import os
import subprocess
import re
import six

re_for_ld = re.compile('.*\((.*)\).*')


def parse_readelf_line(x):
    """Return the version from a readelf line that looks like:
    0x00ec: Rev: 1  Flags: none  Index: 8  Cnt: 2  Name: GLIBCXX_3.4.6
    """
    return x.split(':')[-1].split('_')[-1].strip()


def parse_ld_line(x):
    """Parse a line from the output of ld -t. The output of gold is just
    the full path, gnu ld prints "-lstdc++ (path)".
    """
    t = re_for_ld.match(x)
    if t:
        return t.groups()[0].strip()
    return x.strip()


def split_ver(v):
    """Covert the string '1.2.3' into the list [1,2,3]
    """
    return [int(x) for x in v.split('.')]


def encode_ver(v):
    """Encode the version as a single number.
    """
    t = split_ver(v)
    return t[0] << 16 | t[1] << 8 | t[2]


def find_version(args):
    """Given a base command line for a compiler, find the version of the
    libstdc++ it uses.
    """
    args += ['-shared', '-Wl,-t']
    p = subprocess.Popen(args, stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
    candidates = [six.ensure_text(x, encoding='ascii') for x in p.stdout]
    candidates = [x for x in candidates if 'libstdc++.so' in x]
    candidates = [x for x in candidates if 'skipping incompatible' not in x]
    if not candidates:
        raise Exception('''Couldn't find libstdc++ candidates!
command line: %s''' % args)
    if len(candidates) != 1:
        raise Exception('''Too many libstdc++ candidates!
command line: %s
candidates:
%s''' % (args, '\n'.join(candidates)))

    libstdcxx = parse_ld_line(candidates[-1])

    p = subprocess.Popen(['readelf', '-V', libstdcxx], stdout=subprocess.PIPE)
    lines = [six.ensure_text(x, encoding='ascii') for x in p.stdout]
    versions = [parse_readelf_line(x)
                for x in lines if 'Name: GLIBCXX' in x]
    last_version = sorted(versions, key=split_ver)[-1]
    return (last_version, encode_ver(last_version))


if __name__ == '__main__':
    """Given the value of environment variable CXX or HOST_CXX, find the
    version of the libstdc++ it uses.
    """
    cxx_env = os.environ['CXX']
    print('MOZ_LIBSTDCXX_TARGET_VERSION=%s' % find_version(cxx_env.split())[1])
    host_cxx_env = os.environ.get('HOST_CXX', cxx_env)
    print('MOZ_LIBSTDCXX_HOST_VERSION=%s' % find_version(host_cxx_env.split())[1])
