# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function, unicode_literals

from itertools import combinations
from six.moves import range

import pytest

from mozunit import main
from taskgraph.util import chunking


@pytest.fixture(scope='module')
def mock_manifest_runtimes():
    """Deterministically produce a list of simulated manifest runtimes.

    Args:
        manifests (list): list of manifests against which simulated manifest
            runtimes would be paired up to.

    Returns:
        dict of manifest data paired with a float value representing runtime.
    """
    def inner(manifests):
        manifests = sorted(manifests)
        # Generate deterministic runtime data.
        runtimes = [(i/10) ** (i/10) for i in range(len(manifests))]
        return dict(zip(manifests, runtimes))
    return inner


@pytest.fixture(scope='module')
def unchunked_manifests():
    """Produce a list of unchunked manifests to be consumed by test method.

    Args:
        length (int, optional): number of path elements to keep.
        cutoff (int, optional): number of generated test paths to remove
            from the test set if user wants to limit the number of paths.

    Returns:
        list: list of test paths.
    """
    data = ['blueberry', 'nashi', 'peach', 'watermelon']

    def inner(suite, length=2, cutoff=0):
        if 'web-platform' in suite:
            suffix = ''
            prefix = '/'
        elif 'reftest' in suite:
            suffix = '.list'
            prefix = ''
        else:
            suffix = '.ini'
            prefix = ''
        return [prefix + '/'.join(p) + suffix for p in combinations(data, length)][cutoff:]
    return inner


@pytest.mark.parametrize('platform', ['unix', 'windows', 'android'])
@pytest.mark.parametrize('suite', ['crashtest', 'reftest', 'web-platform-tests', 'xpcshell'])
def test_get_runtimes(platform, suite):
    """Tests that runtime information is returned for known good configurations.
    """
    assert chunking.get_runtimes(platform, suite)


@pytest.mark.parametrize('test_cases', [
        ('nonexistent_platform', 'nonexistent_suite', KeyError),
        ('unix', 'nonexistent_suite', KeyError),
        ('unix', '', TypeError),
        ('', '', TypeError),
        ('', 'nonexistent_suite', TypeError)
    ])
def test_get_runtimes_invalid(test_cases):
    """Ensure get_runtimes() method raises an exception if improper request is made.
    """
    platform = test_cases[0]
    suite = test_cases[1]
    expected = test_cases[2]

    try:
        chunking.get_runtimes(platform, suite)
    except Exception as e:
        assert isinstance(e, expected)


@pytest.mark.parametrize('suite', ['web-platform-tests', 'web-platform-tests-reftests'])
@pytest.mark.parametrize('chunks', [1, 3, 6, 20])
def test_chunk_manifests_wpt(mock_manifest_runtimes, unchunked_manifests, suite, chunks):
    """Tests web-platform-tests and its subsuites chunking process.
    """
    # Setup.
    manifests = unchunked_manifests(suite)

    # Generate the expected results, by generating list of indices that each
    # manifest should go into and then appending each item to that index.
    # This method is intentionally different from the way chunking.py performs
    # chunking for cross-checking.
    expected = [[] for _ in range(chunks)]
    indexed = zip(manifests, list(range(0, chunks)) * len(manifests))
    for i in indexed:
        expected[i[1]].append(i[0])

    # Call the method under test on unchunked manifests.
    chunked_manifests = chunking.chunk_manifests(suite, 'unix', chunks, manifests)

    # Assertions and end test.
    assert chunked_manifests
    if chunks > len(manifests):
        # If chunk count exceeds number of manifests, not all chunks will have
        # manifests.
        with pytest.raises(AssertionError):
            assert all(chunked_manifests)
    else:
        assert all(chunked_manifests)
        minimum = min([len(c) for c in chunked_manifests])
        maximum = max([len(c) for c in chunked_manifests])
        assert maximum - minimum <= 1
        assert expected == chunked_manifests


if __name__ == '__main__':
    main()
