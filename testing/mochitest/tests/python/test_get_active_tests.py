# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import six
import os
from argparse import Namespace
from collections import defaultdict
from textwrap import dedent

from manifestparser import TestManifest

import mozunit
import pytest
from conftest import setup_args


@pytest.fixture
def get_active_tests(setup_test_harness, parser):
    setup_test_harness(*setup_args)
    runtests = pytest.importorskip("runtests")
    md = runtests.MochitestDesktop("plain", {"log_tbpl": "-"})

    options = vars(parser.parse_args([]))

    def inner(**kwargs):
        opts = options.copy()
        opts.update(kwargs)

        manifest = opts.get("manifestFile")
        if isinstance(manifest, six.string_types):
            md.testRootAbs = os.path.dirname(manifest)
        elif isinstance(manifest, TestManifest):
            md.testRootAbs = manifest.rootdir

        md._active_tests = None
        md.prefs_by_manifest = defaultdict(set)
        return md, md.getActiveTests(Namespace(**opts))

    return inner


@pytest.fixture
def create_manifest(tmpdir, build_obj):
    def inner(string, name="manifest.ini"):
        manifest = tmpdir.join(name)
        manifest.write(string, ensure=True)
        # pylint --py3k: W1612
        path = six.text_type(manifest)
        return TestManifest(manifests=(path,), strict=False, rootdir=tmpdir.strpath)

    return inner


def test_prefs_validation(get_active_tests, create_manifest):
    # Test prefs set in a single manifest.
    manifest_relpath = "manifest.ini"
    manifest = create_manifest(
        dedent(
            """
    [DEFAULT]
    prefs=
      foo=bar
      browser.dom.foo=baz

    [files/test_pass.html]
    [files/test_fail.html]
    """
        )
    )

    options = {
        "runByManifest": True,
        "manifestFile": manifest,
    }
    md, tests = get_active_tests(**options)

    assert len(tests) == 2
    assert manifest_relpath in md.prefs_by_manifest

    prefs = md.prefs_by_manifest[manifest_relpath]
    assert len(prefs) == 1
    assert prefs.pop() == "\nfoo=bar\nbrowser.dom.foo=baz"

    # Test prefs set with runByManifest disabled.
    options["runByManifest"] = False
    with pytest.raises(SystemExit):
        get_active_tests(**options)

    # Test prefs set in non-default section.
    options["runByManifest"] = True
    options["manifestFile"] = create_manifest(
        dedent(
            """
    [files/test_pass.html]
    prefs=foo=bar
    [files/test_fail.html]
    """
        )
    )
    with pytest.raises(SystemExit):
        get_active_tests(**options)


def test_prefs_validation_with_ancestor_manifest(get_active_tests, create_manifest):
    # Test prefs set by an ancestor manifest.
    create_manifest(
        dedent(
            """
    [DEFAULT]
    prefs=
      foo=bar
      browser.dom.foo=baz

    [files/test_pass.html]
    [files/test_fail.html]
    """
        ),
        name="subdir/manifest.ini",
    )

    manifest = create_manifest(
        dedent(
            """
    [DEFAULT]
    prefs =
      browser.dom.foo=fleem
      flower=rose

    [include:manifest.ini]
    [test_foo.html]
    """
        ),
        name="subdir/ancestor-manifest.ini",
    )

    options = {
        "runByManifest": True,
        "manifestFile": manifest,
    }

    md, tests = get_active_tests(**options)
    assert len(tests) == 3

    key = os.path.join("subdir", "ancestor-manifest.ini")
    assert key in md.prefs_by_manifest
    prefs = md.prefs_by_manifest[key]
    assert len(prefs) == 1
    assert prefs.pop() == "\nbrowser.dom.foo=fleem\nflower=rose"

    key = "{}:{}".format(
        os.path.join("subdir", "ancestor-manifest.ini"),
        os.path.join("subdir", "manifest.ini"),
    )
    assert key in md.prefs_by_manifest
    prefs = md.prefs_by_manifest[key]
    assert len(prefs) == 1
    assert (
        prefs.pop()
        == "\nbrowser.dom.foo=fleem\nflower=rose \nfoo=bar\nbrowser.dom.foo=baz"
    )


if __name__ == "__main__":
    mozunit.main()
