from __future__ import absolute_import

from mozlog import get_proxy_logger
from .mitmproxy import Mitmproxy

LOG = get_proxy_logger(component='mitmproxy')

playback_cls = {
    'mitmproxy': Mitmproxy,
}


def get_playback(config):
    tool_name = config.get('playback_tool', None)
    if tool_name is None:
        LOG.critical("playback_tool name not found in config")
        return
    if playback_cls.get(tool_name, None) is None:
        LOG.critical("specified playback tool is unsupported: %s" % tool_name)
        return None

    cls = playback_cls.get(tool_name)
    return cls(config)
