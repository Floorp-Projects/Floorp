#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import zipfile

import mozunit
import pytest

from mozversion import get_version


"""test getting version information from an android .apk"""

application_changeset = 'a' * 40
platform_changeset = 'b' * 40


@pytest.fixture(name='apk')
def fixture_apk(tmpdir):
    path = str(tmpdir.join('apk.zip'))
    with zipfile.ZipFile(path, 'w') as z:
        z.writestr('application.ini',
                   """[App]\nSourceStamp=%s\n""" % application_changeset)
        z.writestr('platform.ini',
                   """[Build]\nSourceStamp=%s\n""" % platform_changeset)
        z.writestr('AndroidManifest.xml', '')
    return path


def test_basic(apk):
    v = get_version(apk)
    assert v.get('application_changeset') == application_changeset
    assert v.get('platform_changeset') == platform_changeset


def test_with_package_name(apk):
    with zipfile.ZipFile(apk, 'a') as z:
        z.writestr('package-name.txt', 'org.mozilla.fennec')
    v = get_version(apk)
    assert v.get('package_name') == 'org.mozilla.fennec'


if __name__ == '__main__':
    mozunit.main()
