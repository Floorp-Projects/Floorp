#!/usr/bin/env python

from __future__ import absolute_import

import mozprofile
import os
import shutil

import mozunit

here = os.path.dirname(os.path.abspath(__file__))

"""
use of --profile in mozrunner just blows away addon sources:
https://bugzilla.mozilla.org/show_bug.cgi?id=758250
"""


def test_profile_addon_cleanup(tmpdir):
    tmpdir = tmpdir.mkdtemp().strpath
    addon = os.path.join(here, 'addons', 'empty')

    # sanity check: the empty addon should be here
    assert os.path.exists(addon)
    assert os.path.isdir(addon)
    assert os.path.exists(os.path.join(addon, 'install.rdf'))

    # because we are testing data loss, let's make sure we make a copy
    shutil.rmtree(tmpdir)
    shutil.copytree(addon, tmpdir)
    assert os.path.exists(os.path.join(tmpdir, 'install.rdf'))

    # make a starter profile
    profile = mozprofile.FirefoxProfile()
    path = profile.profile

    # make a new profile based on the old
    newprofile = mozprofile.FirefoxProfile(profile=path, addons=[tmpdir])
    newprofile.cleanup()

    # the source addon *should* still exist
    assert os.path.exists(tmpdir)
    assert os.path.exists(os.path.join(tmpdir, 'install.rdf'))


if __name__ == '__main__':
    mozunit.main()
