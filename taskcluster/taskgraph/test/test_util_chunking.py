# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function, unicode_literals

import re

from itertools import combinations
from six.moves import range

import pytest

from mozunit import main
from taskgraph.util import chunking


pytestmark = pytest.mark.slow


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


@pytest.fixture(scope='module')
def mock_task_definition():
    """Builds a mock task definition for use in testing.

    Args:
        platform (str): represents the build platform.
        bits (int): software bits.
        build_type (str): opt or debug.
        suite (str): name of the unittest suite.
        test_platform (str, optional): full name of the platform and major version.
        variant (str, optional): specify fission or vanilla.

    Returns:
        dict: mocked task definition.
    """
    def inner(platform, bits, build_type, suite, test_platform='', variant=''):
        bits = str(bits)
        test_variant = [suite, 'e10s']
        if 'fis' in variant:
            test_variant.insert(1, 'fis')
        output = {
            'build-attributes': {
                'build_platform': platform + bits,
                'build_type': build_type,

            },
            'attributes': {
                'e10s': True,
                'unittest_variant': variant
            },
            'test-name': '-'.join(test_variant),
            'test-platform': ''.join([test_platform, '-', bits, '/', build_type])
        }
        return output
    return inner


@pytest.fixture(scope='module')
def mock_mozinfo():
    """Returns a mocked mozinfo object, similar to guess_mozinfo_from_task().

    Args:
        os (str): typically one of 'win, linux, mac, android'.
        processor (str): processor architecture.
        asan (bool, optional): addressanitizer build.
        bits (int, optional): defaults to 64.
        ccov (bool, optional): code coverage build.
        debug (bool, optional): debug type build.
        fission (bool, optional): process fission.
        headless (bool, optional): headless browser testing without displays.
        tsan (bool, optional): threadsanitizer build.

    Returns:
        dict: Dictionary mimickign the results from guess_mozinfo_from_task.
    """
    def inner(os, processor, asan=False, bits=64, ccov=False, debug=False,
              fission=False, headless=False, tsan=False):
        return {
            'os': os,
            'processor': processor,
            'toolkit': '',
            'asan': asan,
            'bits': bits,
            'ccov': ccov,
            'debug': debug,
            'e10s': True,
            'fission': fission,
            'headless': headless,
            'tsan': tsan,
            'webrender': False,
            'appname': 'firefox',
        }
    return inner


@pytest.mark.parametrize('params,exception', [
    [('win', 32, 'opt', 'web-platform-tests', '', ''), None],
    [('win', 64, 'opt', 'web-platform-tests', '', ''), None],
    [('linux', 64, 'debug', 'mochitest-plain', '', ''), None],
    [('mac', 64, 'debug', 'mochitest-plain', '', ''), None],
    [('mac', 64, 'opt', 'mochitest-plain-headless', '', ''), None],
    [('android', 64, 'debug', 'xpcshell', '', ''), None],
    [('and', 64, 'debug', 'awsy', '', ''), ValueError],
    [('', 64, 'opt', '', '', ''), ValueError],
    [('linux-ccov', 64, 'opt', '', '', ''), None],
    [('linux-asan', 64, 'opt', '', '', ''), None],
    [('win-tsan', 64, 'opt', '', '', ''), None],
    [('mac-ccov', 64, 'opt', '', '', ''), None],
    [('android', 64, 'opt', 'crashtest', 'arm64', 'fission'), None],
    [('win-aarch64', 64, 'opt', 'crashtest', '', ''), None],
])
def test_guess_mozinfo_from_task(params, exception, mock_task_definition):
    """Tests the mozinfo guessing process.
    """
    # Set up a mocked task object.
    task = mock_task_definition(*params)

    if exception:
        with pytest.raises(exception):
            result = chunking.guess_mozinfo_from_task(task)
    else:
        result = chunking.guess_mozinfo_from_task(task)

        assert result['bits'] == (32 if '32' in task['test-platform'] else 64)
        assert result['os'] in ('android', 'linux', 'mac', 'win')

        # Ensure the outcome of special build variants being present match what
        # guess_mozinfo_from_task method returns for these attributes.
        assert ('asan' in task['build-attributes']['build_platform']) == result['asan']
        assert ('tsan' in task['build-attributes']['build_platform']) == result['tsan']
        assert ('ccov' in task['build-attributes']['build_platform']) == result['ccov']

        assert result['fission'] == any(task['attributes']['unittest_variant'])
        assert result['e10s']


@pytest.mark.parametrize('platform', ['unix', 'windows', 'android'])
@pytest.mark.parametrize('suite', ['crashtest', 'reftest', 'web-platform-tests', 'xpcshell'])
def test_get_runtimes(platform, suite):
    """Tests that runtime information is returned for known good configurations.
    """
    assert chunking.get_runtimes(platform, suite)


@pytest.mark.parametrize('platform,suite,exception', [
        ('nonexistent_platform', 'nonexistent_suite', KeyError),
        ('unix', 'nonexistent_suite', KeyError),
        ('unix', '', TypeError),
        ('', '', TypeError),
        ('', 'nonexistent_suite', TypeError),
    ])
def test_get_runtimes_invalid(platform, suite, exception):
    """Ensure get_runtimes() method raises an exception if improper request is made.
    """
    with pytest.raises(exception):
        chunking.get_runtimes(platform, suite)


@pytest.mark.parametrize('suite', [
    'web-platform-tests',
    'web-platform-tests-reftest',
    'web-platform-tests-wdspec',
    'web-platform-tests-crashtest',
])
@pytest.mark.parametrize('chunks', [1, 3, 6, 20])
def test_mock_chunk_manifests_wpt(unchunked_manifests, suite, chunks):
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


@pytest.mark.parametrize('suite', [
    'mochitest-devtools-chrome',
    'mochitest-browser-chrome',
    'mochitest-plain',
    'mochitest-chrome',
    'xpcshell',
])
@pytest.mark.parametrize('chunks', [1, 3, 6, 20])
def test_mock_chunk_manifests(mock_manifest_runtimes, unchunked_manifests, suite, chunks):
    """Tests non-WPT tests and its subsuites chunking process.
    """
    # Setup.
    manifests = unchunked_manifests(suite)

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


@pytest.mark.parametrize('suite', [
    'web-platform-tests',
    'web-platform-tests-reftest',
    'xpcshell',
    'mochitest-plain',
    'mochitest-devtools-chrome',
    'mochitest-browser-chrome',
    'mochitest-chrome',
])
@pytest.mark.parametrize('platform', [
    ('mac', 'x86_64'),
    ('win', 'x86_64'),
    ('win', 'x86'),
    ('win', 'aarch64'),
    ('linux', 'x86_64'),
    ('linux', 'x86'),
])
def test_get_manifests(suite, platform, mock_mozinfo):
    """Tests the DefaultLoader class' ability to load manifests.
    """
    mozinfo = mock_mozinfo(*platform)

    loader = chunking.DefaultLoader([])
    manifests = loader.get_manifests(suite, frozenset(mozinfo.items()))

    assert manifests
    assert manifests['active']
    if 'web-platform' in suite:
        assert manifests['skipped'] == []
    else:
        assert manifests['skipped']

    items = manifests['active']
    if suite == 'xpcshell':
        assert all([re.search(r'xpcshell(.*)?.ini', m) for m in items])
    if 'mochitest' in suite:
        assert all([re.search(r'(mochitest|chrome|browser).*.ini', m) for m in items])
    if 'web-platform' in suite:
        assert all([m.startswith('/') and m.count('/') <= 4 for m in items])


@pytest.mark.parametrize('suite', [
    'mochitest-devtools-chrome',
    'mochitest-browser-chrome',
    'mochitest-plain',
    'mochitest-chrome',
    'web-platform-tests',
    'web-platform-tests-reftest',
    'xpcshell',
])
@pytest.mark.parametrize('platform', [
    ('mac', 'x86_64'),
    ('win', 'x86_64'),
    ('linux', 'x86_64'),
])
@pytest.mark.parametrize('chunks', [1, 3, 6, 20])
def test_chunk_manifests(suite, platform, chunks, mock_mozinfo):
    """Tests chunking with real manifests.
    """
    mozinfo = mock_mozinfo(*platform)

    loader = chunking.DefaultLoader([])
    manifests = loader.get_manifests(suite, frozenset(mozinfo.items()))

    chunked_manifests = chunking.chunk_manifests(suite, platform, chunks,
                                                 manifests['active'])

    # Assertions and end test.
    assert chunked_manifests
    assert len(chunked_manifests) == chunks
    assert all(chunked_manifests)


if __name__ == '__main__':
    main()
