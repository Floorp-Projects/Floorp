# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
from argparse import Namespace

from manifestparser import TestManifest

import mozunit
import pytest


@pytest.fixture
def get_active_tests(setup_harness_root, parser):
    runtests = pytest.importorskip('runtests')
    md = runtests.MochitestDesktop('plain', {'log_tbpl': '-'})

    options = vars(parser.parse_args([]))

    def inner(**kwargs):
        opts = options.copy()
        opts.update(kwargs)

        manifest = opts.get('manifestFile')
        if isinstance(manifest, basestring):
            md.testRootAbs = os.path.dirname(manifest)
        elif isinstance(manifest, TestManifest):
            md.testRootAbs = manifest.rootdir

        md._active_tests = None
        return md, md.getActiveTests(Namespace(**opts))

    return inner


@pytest.fixture
def create_manifest(tmpdir, build_obj):
    manifest = tmpdir.join('manifest.ini')

    def inner(string):
        manifest.write(string)
        path = unicode(manifest)
        mobj = TestManifest(manifests=(path,), strict=False)
        manifest_root = build_obj.topsrcdir if build_obj else mobj.rootdir
        return os.path.relpath(path, manifest_root), mobj
    return inner


def test_prefs_validation(get_active_tests, create_manifest):
    manifest_relpath, manifest = create_manifest("""
[DEFAULT]
prefs=
  foo=bar
  browser.dom.foo=baz

[files/test_pass.html]
[files/test_fail.html]
""")

    options = {
        'runByManifest': True,
        'manifestFile': manifest,
    }
    md, tests = get_active_tests(**options)

    assert len(tests) == 2
    assert manifest_relpath in md.prefs_by_manifest

    prefs = md.prefs_by_manifest[manifest_relpath]
    assert len(prefs) == 1
    assert prefs.pop() == "\nfoo=bar\nbrowser.dom.foo=baz"

    options['runByManifest'] = False
    with pytest.raises(SystemExit):
        get_active_tests(**options)

    options['runByManifest'] = True
    options['manifestFile'] = create_manifest("""
[files/test_pass.html]
prefs=foo=bar
[files/test_fail.html]
""")[1]
    with pytest.raises(SystemExit):
        get_active_tests(**options)


if __name__ == '__main__':
    mozunit.main()
