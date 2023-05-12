# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import tempfile
from unittest import mock

import mozunit
import pytest
from tryselect.selectors.perfselector.perfcomparators import (
    BadComparatorArgs,
    BenchmarkComparator,
    ComparatorNotFound,
    get_comparator,
)


@pytest.mark.parametrize(
    "test_link",
    [
        "https://github.com/mozilla-mobile/firefox-android/pull/1627",
        "https://github.com/mozilla-mobile/firefox-android/pull/1876/"
        "commits/17c7350cc37a4a85cea140a7ce54e9fd037b5365",
    ],
)
def test_benchmark_comparator(test_link):
    def _verify_extra_args(extra_args):
        assert len(extra_args) == 3
        if "commit" in test_link:
            assert (
                "benchmark-revision=17c7350cc37a4a85cea140a7ce54e9fd037b5365"
                in extra_args
            )
        else:
            assert "benchmark-revision=sha-for-link" in extra_args
        assert "benchmark-repository=url-for-link" in extra_args
        assert "benchmark-branch=ref-for-link" in extra_args

    comparator = BenchmarkComparator(
        None, None, None, [f"base-link={test_link}", f"new-link={test_link}"]
    )

    with mock.patch("requests.get") as mocked_get:
        magic_get = mock.MagicMock()
        magic_get.json.return_value = {
            "head": {
                "repo": {
                    "html_url": "url-for-link",
                },
                "sha": "sha-for-link",
                "ref": "ref-for-link",
            }
        }
        magic_get.status_code = 200
        mocked_get.return_value = magic_get

        extra_args = []
        comparator.setup_base_revision(extra_args)
        _verify_extra_args(extra_args)

        extra_args = []
        comparator.setup_new_revision(extra_args)
        _verify_extra_args(extra_args)


def test_benchmark_comparator_no_pr_links():
    def _verify_extra_args(extra_args):
        assert len(extra_args) == 3
        assert "benchmark-revision=rev" in extra_args
        assert "benchmark-repository=link" in extra_args
        assert "benchmark-branch=fake" in extra_args

    comparator = BenchmarkComparator(
        None,
        None,
        None,
        [
            "base-repo=link",
            "base-branch=fake",
            "base-revision=rev",
            "new-repo=link",
            "new-branch=fake",
            "new-revision=rev",
        ],
    )

    with mock.patch("requests.get") as mocked_get:
        magic_get = mock.MagicMock()
        magic_get.json.return_value = {
            "head": {
                "repo": {
                    "html_url": "url-for-link",
                },
                "sha": "sha-for-link",
                "ref": "ref-for-link",
            }
        }
        magic_get.status_code = 200
        mocked_get.return_value = magic_get

        extra_args = []
        comparator.setup_base_revision(extra_args)
        _verify_extra_args(extra_args)

        extra_args = []
        comparator.setup_new_revision(extra_args)
        _verify_extra_args(extra_args)


def test_benchmark_comparator_bad_args():
    comparator = BenchmarkComparator(
        None,
        None,
        None,
        [
            "base-bad-args=val",
        ],
    )

    with pytest.raises(BadComparatorArgs):
        comparator.setup_base_revision([])


def test_get_comparator_bad_name():
    with pytest.raises(ComparatorNotFound):
        get_comparator("BadName")


def test_get_comparator_bad_script():
    with pytest.raises(ComparatorNotFound):
        with tempfile.NamedTemporaryFile() as tmpf:
            tmpf.close()
            get_comparator(tmpf.name)


def test_get_comparator_benchmark_name():
    comparator_klass = get_comparator("BenchmarkComparator")
    assert comparator_klass.__name__ == "BenchmarkComparator"


def test_get_comparator_benchmark_script():
    # If the get_comparator method is working for scripts, then
    # it should find the first defined class in this file, or the
    # first imported class that matches it
    comparator_klass = get_comparator(__file__)
    assert comparator_klass.__name__ == "BenchmarkComparator"


if __name__ == "__main__":
    mozunit.main()
