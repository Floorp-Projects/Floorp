'''This helps loading mitmproxy's cert and change proxy settings for Firefox.'''
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import os

from mozlog import get_proxy_logger

from .base import Playback

here = os.path.dirname(os.path.realpath(__file__))
tooltool_cache = os.path.join(here, 'tooltoolcache')

LOG = get_proxy_logger(component='mitmproxy')


class Mitmproxy(Playback):

    def __init__(self, config):
        self.config = config
        self.download()
        self.setup()

    def download(self):
        LOG.info("todo: download mitmproxy release binary")
        return

    def setup(self):
        LOG.info("todo: setup mitmproxy")
        return

    def start(self):
        LOG.info("todo: start mitmproxy playback")
        return

    def stop(self):
        LOG.info("todo: stop mitmproxy playback")
        return
