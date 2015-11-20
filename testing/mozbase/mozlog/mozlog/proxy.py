# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from .structuredlog import get_default_logger


class ProxyLogger(object):
    """
    A ProxyLogger behaves like a
    :class:`mozlog.structuredlog.StructuredLogger`.

    Each method and attribute access will be forwarded to the underlying
    StructuredLogger.

    RuntimeError will be raised when the default logger is not yet initialized.
    """
    def __init__(self, component=None):
        self.logger = None
        self._component = component

    def __getattr__(self, name):
        if self.logger is None:
            self.logger = get_default_logger(component=self._component)
            if self.logger is None:
                raise RuntimeError("Default logger is not initialized!")
        return getattr(self.logger, name)


def get_proxy_logger(component=None):
    """
    Returns a :class:`ProxyLogger` for the given component.
    """
    return ProxyLogger(component)
