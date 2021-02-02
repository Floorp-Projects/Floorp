# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import six

from abc import ABCMeta, abstractmethod


# abstract class for all playback tools
@six.add_metaclass(ABCMeta)
class Playback(object):
    def __init__(self, config):
        self.config = config
        self.host = None
        self.port = None

    @abstractmethod
    def download(self):
        pass

    @abstractmethod
    def setup(self):
        pass

    @abstractmethod
    def start(self):
        pass

    @abstractmethod
    def stop(self):
        pass
