from __future__ import absolute_import, unicode_literals

import mozinfo
import mozunit
import os
import pytest
import sys

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../mozharness')
sys.path.insert(1, mozharness_dir)

raptor_dir = os.path.join(os.path.dirname(here), 'raptor')
sys.path.insert(0, raptor_dir)

from manifest import get_browser_test_list, validate_test_ini, get_raptor_test_list


# some test details (test INIs)
VALID_MANIFESTS = [{'apps': 'firefox',
                    'type': 'pageload',
                    'page_cycles': 25,
                    'test_url': 'http://www.test-url/goes/here',
                    'measure': 'fnbpaint, fcp',
                    'alert_on': 'fcp',
                    'unit': 'ms',
                    'lower_is_better': True,
                    'alert_threshold': 2.0,
                    'playback': 'mitmproxy',
                    'playback_binary_manifest': 'binary.manifest',
                    'playback_pageset_manifest': 'pageset.manifest',
                    'playback_recordings': 'recorded_site.mp',
                    'python3_win_manifest': 'py3.manifest',
                    'manifest': 'valid_details_1'},
                   {'apps': 'chrome',
                    'type': 'benchmark',
                    'page_cycles': 5,
                    'test_url': 'http://www.test-url/goes/here',
                    'measure': 'fcp',
                    'unit': 'score',
                    'lower_is_better': False,
                    'alert_threshold': 2.0,
                    'manifest': 'valid_details_2'}]

INVALID_MANIFESTS = [{'apps': 'firefox',
                      'type': 'pageload',
                      'page_cycles': 25,
                      'test_url': 'http://www.test-url/goes/here',
                      'unit': 'ms',
                      'lower_is_better': True,
                      'alert_threshold': 2.0,
                      'playback': 'mitmproxy',
                      'playback_binary_manifest': 'binary.manifest',
                      'playback_pageset_manifest': 'pageset.manifest',
                      'playback_recordings': 'recorded_site.mp',
                      'python3_win_manifest': 'py3.manifest',
                      'manifest': 'invalid_details_1'},
                     {'apps': 'chrome',
                      'type': 'pageload',
                      'page_cycles': 25,
                      'test_url': 'http://www.test-url/goes/here',
                      'measure': 'fnbpaint, fcp',
                      'unit': 'ms',
                      'lower_is_better': True,
                      'alert_threshold': 2.0,
                      'playback': 'mitmproxy',
                      'manifest': 'invalid_details_2'},
                     {'apps': 'firefox',
                      'type': 'pageload',
                      'page_cycles': 25,
                      'test_url': 'http://www.test-url/goes/here',
                      'measure': 'fnbpaint, fcp',
                      'alert_on': 'nope',
                      'unit': 'ms',
                      'lower_is_better': True,
                      'alert_threshold': 2.0,
                      'playback': 'mitmproxy',
                      'playback_binary_manifest': 'binary.manifest',
                      'playback_pageset_manifest': 'pageset.manifest',
                      'playback_recordings': 'recorded_site.mp',
                      'python3_win_manifest': 'py3.manifest',
                      'manifest': 'invalid_details_3'}]


@pytest.mark.parametrize('app', ['firefox', 'chrome', 'geckoview'])
def test_get_browser_test_list(app):
    test_list = get_browser_test_list(app)
    assert len(test_list) > 0


@pytest.mark.parametrize('test_details', VALID_MANIFESTS)
def test_validate_test_ini_valid(test_details):
    assert(validate_test_ini(test_details))


@pytest.mark.parametrize('test_details', INVALID_MANIFESTS)
def test_validate_test_ini_invalid(test_details):
    assert not (validate_test_ini(test_details))


def test_get_raptor_test_list_firefox(create_args):
    args = create_args()

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 4

    subtests = ['raptor-tp6-google-firefox', 'raptor-tp6-amazon-firefox',
                'raptor-tp6-facebook-firefox', 'raptor-tp6-youtube-firefox']

    for next_subtest in test_list:
        assert next_subtest['name'] in subtests


def test_get_raptor_test_list_chrome(create_args):
    args = create_args(app="chrome",
                       test="raptor-speedometer")

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-speedometer-chrome'


def test_get_raptor_test_list_geckoview(create_args):
    args = create_args(app="geckoview",
                       test="raptor-unity-webgl")

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-unity-webgl-geckoview'


def test_get_raptor_test_list_gecko_profiling(create_args):
    args = create_args(test="raptor-tp6-google-firefox",
                       gecko_profile=True)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-tp6-google-firefox'
    assert test_list[0]['gecko_profile'] is True
    assert test_list[0]['page_cycles'] == 3


def test_get_raptor_test_list_debug_mode(create_args):
    args = create_args(test="raptor-tp6-google-firefox",
                       debug_mode=True)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-tp6-google-firefox'
    assert test_list[0]['debug_mode'] is True
    assert test_list[0]['page_cycles'] == 2


def test_get_raptor_test_list_override_page_cycles(create_args):
    args = create_args(test="raptor-tp6-google-firefox",
                       page_cycles=99)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-tp6-google-firefox'
    assert test_list[0]['page_cycles'] == 99


def test_get_raptor_test_list_override_page_timeout(create_args):
    args = create_args(test="raptor-tp6-google-firefox",
                       page_timeout=9999)

    test_list = get_raptor_test_list(args, mozinfo.os)
    assert len(test_list) == 1
    assert test_list[0]['name'] == 'raptor-tp6-google-firefox'
    assert test_list[0]['page_timeout'] == 9999


if __name__ == '__main__':
    mozunit.main()
