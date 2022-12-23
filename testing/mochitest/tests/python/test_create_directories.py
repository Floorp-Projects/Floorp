# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest.mock as mock
from argparse import Namespace
from collections import defaultdict
from textwrap import dedent

import mozunit
import pytest
import six
from conftest import setup_args
from manifestparser import TestManifest


# Directly running runTests() is likely not working nor a good idea
# So at least we try to minimize with just:
#  - getActiveTests()
#  - create manifests list
#  - parseAndCreateTestsDirs()
#
# Hopefully, breaking the runTests() calls to parseAndCreateTestsDirs() will
# anyway trigger other tests failures so it would be spotted, and we at least
# ensure some coverage of handling the manifest content, creation of the
# directories and cleanup
@pytest.fixture
def prepareRunTests(setup_test_harness, parser):
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
        tests = md.getActiveTests(Namespace(**opts))
        manifests = set(t["manifest"] for t in tests)
        for m in sorted(manifests):
            md.parseAndCreateTestsDirs(m)
        return md

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


def create_manifest_empty(create_manifest):
    manifest = create_manifest(
        dedent(
            """
    [DEFAULT]
    [files/test_pass.html]
    [files/test_fail.html]
    """
        )
    )

    return {
        "runByManifest": True,
        "manifestFile": manifest,
    }


def create_manifest_one(create_manifest):
    manifest = create_manifest(
        dedent(
            """
    [DEFAULT]
    test-directories =
        .snap_firefox_current_real
    [files/test_pass.html]
    [files/test_fail.html]
    """
        )
    )

    return {
        "runByManifest": True,
        "manifestFile": manifest,
    }


def create_manifest_mult(create_manifest):
    manifest = create_manifest(
        dedent(
            """
    [DEFAULT]
    test-directories =
        .snap_firefox_current_real
        .snap_firefox_current_real2
    [files/test_pass.html]
    [files/test_fail.html]
    """
        )
    )

    return {
        "runByManifest": True,
        "manifestFile": manifest,
    }


def test_no_entry(prepareRunTests, create_manifest):
    options = create_manifest_empty(create_manifest)
    with mock.patch("os.makedirs") as mock_os_makedirs:
        _ = prepareRunTests(**options)
        mock_os_makedirs.assert_not_called()


def test_one_entry(prepareRunTests, create_manifest):
    options = create_manifest_one(create_manifest)
    with mock.patch("os.makedirs") as mock_os_makedirs:
        md = prepareRunTests(**options)
        mock_os_makedirs.assert_called_once_with(".snap_firefox_current_real")

        opts = mock.Mock(pidFile="")  # so cleanup() does not check it
        with mock.patch("os.path.exists") as mock_os_path_exists, mock.patch(
            "shutil.rmtree"
        ) as mock_shutil_rmtree:
            md.cleanup(opts, False)
            mock_os_path_exists.assert_called_once_with(".snap_firefox_current_real")
            mock_shutil_rmtree.assert_called_once_with(".snap_firefox_current_real")


def test_one_entry_already_exists(prepareRunTests, create_manifest):
    options = create_manifest_one(create_manifest)
    with mock.patch(
        "os.path.exists", return_value=True
    ) as mock_os_path_exists, mock.patch("os.makedirs") as mock_os_makedirs:
        with pytest.raises(FileExistsError):
            _ = prepareRunTests(**options)
        mock_os_path_exists.assert_called_once_with(".snap_firefox_current_real")
        mock_os_makedirs.assert_not_called()


def test_mult_entry(prepareRunTests, create_manifest):
    options = create_manifest_mult(create_manifest)
    with mock.patch("os.makedirs") as mock_os_makedirs:
        md = prepareRunTests(**options)
        assert mock_os_makedirs.call_count == 2
        mock_os_makedirs.assert_has_calls(
            [
                mock.call(".snap_firefox_current_real"),
                mock.call(".snap_firefox_current_real2"),
            ]
        )

        opts = mock.Mock(pidFile="")  # so cleanup() does not check it
        with mock.patch("os.path.exists") as mock_os_path_exists, mock.patch(
            "shutil.rmtree"
        ) as mock_shutil_rmtree:
            md.cleanup(opts, False)

            assert mock_os_path_exists.call_count == 2
            mock_os_path_exists.assert_has_calls(
                [
                    mock.call(".snap_firefox_current_real"),
                    mock.call().__bool__(),
                    mock.call(".snap_firefox_current_real2"),
                    mock.call().__bool__(),
                ]
            )

            assert mock_os_makedirs.call_count == 2
            mock_shutil_rmtree.assert_has_calls(
                [
                    mock.call(".snap_firefox_current_real"),
                    mock.call(".snap_firefox_current_real2"),
                ]
            )


def test_mult_entry_one_already_exists(prepareRunTests, create_manifest):
    options = create_manifest_mult(create_manifest)
    with mock.patch(
        "os.path.exists", side_effect=[True, False]
    ) as mock_os_path_exists, mock.patch("os.makedirs") as mock_os_makedirs:
        with pytest.raises(FileExistsError):
            _ = prepareRunTests(**options)
        mock_os_path_exists.assert_called_once_with(".snap_firefox_current_real")
        mock_os_makedirs.assert_not_called()

    with mock.patch(
        "os.path.exists", side_effect=[False, True]
    ) as mock_os_path_exists, mock.patch("os.makedirs") as mock_os_makedirs:
        with pytest.raises(FileExistsError):
            _ = prepareRunTests(**options)
        assert mock_os_path_exists.call_count == 2
        mock_os_path_exists.assert_has_calls(
            [
                mock.call(".snap_firefox_current_real"),
                mock.call(".snap_firefox_current_real2"),
            ]
        )
        mock_os_makedirs.assert_not_called()


if __name__ == "__main__":
    mozunit.main()
