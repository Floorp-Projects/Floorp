from __future__ import absolute_import, unicode_literals

import mozunit

from mozlog.structuredlog import set_default_logger, StructuredLogger

set_default_logger(StructuredLogger('test_playback'))

from raptor.playback import get_playback, Mitmproxy


config = {}


def test_get_playback(get_binary):
    config['playback_tool'] = 'mitmproxy'
    config['playback_binary_manifest'] = 'mitmproxy-rel-bin-osx.manifest'
    config['playback_binary_zip_mac'] = 'mitmproxy-2.0.2-osx.tar.gz'
    config['playback_pageset_manifest'] = 'mitmproxy-playback-set.manifest'
    config['playback_pageset_zip_mac'] = 'mitmproxy-recording-set-win10.zip'
    config['playback_recordings'] = 'mitmproxy-recording-amazon.mp'
    config['binary'] = get_binary('firefox')
    playback = get_playback(config)
    assert isinstance(playback, Mitmproxy)
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
