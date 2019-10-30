#!/usr/bin/env python
# Copyright 2015,2016 Nir Cohen
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

from pprint import pformat

import distro


def pprint(obj):
    for line in pformat(obj).split('\n'):
        print(4 * ' ' + line)


print('os_release_info:')
pprint(distro.os_release_info())
print('lsb_release_info:')
pprint(distro.lsb_release_info())
print('distro_release_info:')
pprint(distro.distro_release_info())
print('id: {0}'.format(distro.id()))
print('name: {0}'.format(distro.name()))
print('name_pretty: {0}'.format(distro.name(True)))
print('version: {0}'.format(distro.version()))
print('version_pretty: {0}'.format(distro.version(True)))
print('like: {0}'.format(distro.like()))
print('codename: {0}'.format(distro.codename()))
print('linux_distribution_full: {0}'.format(distro.linux_distribution()))
print('linux_distribution: {0}'.format(distro.linux_distribution(False)))
print('major_version: {0}'.format(distro.major_version()))
print('minor_version: {0}'.format(distro.minor_version()))
print('build_number: {0}'.format(distro.build_number()))
