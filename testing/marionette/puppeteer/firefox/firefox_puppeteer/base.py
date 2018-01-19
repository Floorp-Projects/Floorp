# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import


class BaseLib(object):
    """A base class that handles lazily setting the "client" class attribute."""

    def __init__(self, marionette):
        self._marionette = marionette

    @property
    def marionette(self):
        return self._marionette
