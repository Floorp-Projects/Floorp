from __future__ import absolute_import, unicode_literals

import os
import time

import mozinfo
import mozunit

from mozlog.structuredlog import set_default_logger, StructuredLogger

set_default_logger(StructuredLogger('test_playback'))

from mozproxy import get_playback
from mozproxy.backends.mitm import MitmproxyDesktop

config = {}

run_local = True
if os.environ.get('TOOLTOOLCACHE') is None:
    run_local = False


def test_get_playback(get_binary):
    config['platform'] = mozinfo.os
    if 'win' in config['platform']:
        # this test is not yet supported on windows
        assert True
        return
    config['obj_path'] = os.path.dirname(get_binary('firefox'))
    config['playback_tool'] = 'mitmproxy'
    config['playback_version'] = '4.0.4'
    config['playback_upstream_cert'] = 'false'
    config['playback_binary_manifest'] = 'mitmproxy-rel-bin-4.0.4-{platform}.manifest'
    config['playback_pageset_manifest'] = os.path.join(
        os.path.dirname(os.path.abspath(os.path.dirname(__file__))), "raptor", "playback",
        'mitm4-linux-firefox-amazon.manifest')
    config['playback_recordings'] = 'amazon.mp'
    config['binary'] = get_binary('firefox')
    config['run_local'] = run_local
    config['app'] = 'firefox'
    config['host'] = 'https://www.amazon.com/s?k=laptop&ref=nb_sb_noss_1'

    playback = get_playback(config)
    playback.config['playback_files'] = [os.path.join(playback.mozproxy_dir,
                                                      config['playback_recordings'])]
    assert isinstance(playback, MitmproxyDesktop)
    playback.start()
    time.sleep(1)
    playback.stop()


def test_get_unsupported_playback():
    config['playback_tool'] = 'unsupported'
    playback = get_playback(config)
    assert playback is None


def test_get_playback_missing_tool_name():
    playback = get_playback(config)
    assert playback is None


if __name__ == '__main__':
    mozunit.main()
