from __future__ import absolute_import, unicode_literals

import mozunit

from mozlog.structuredlog import set_default_logger, StructuredLogger

set_default_logger(StructuredLogger('test_playback'))

from raptor.playback import get_playback, Mitmproxy


config = {}


def test_get_playback():
    config['playback_tool'] = 'mitmproxy'
    playback = get_playback(config)
    assert isinstance(playback, Mitmproxy)


def test_get_unsupported_playback():
    config['playback_tool'] = 'unsupported'
    playback = get_playback(config)
    assert playback is None


def test_get_playback_missing_tool_name():
    playback = get_playback(config)
    assert playback is None


if __name__ == '__main__':
    mozunit.main()
