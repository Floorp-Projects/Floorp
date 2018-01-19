# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

from functools import wraps
from importlib import import_module


class use_class_as_property(object):
    """
    This decorator imports a library module and sets an instance
    of the associated class as an attribute on the Puppeteer
    object and returns it.

    Note: return value of the wrapped function is ignored.
    """
    def __init__(self, lib):
        self.lib = lib
        self.mod_name, self.cls_name = self.lib.rsplit('.', 1)

    def __call__(self, func):
        @property
        @wraps(func)
        def _(cls, *args, **kwargs):
            tag = '_{}_{}'.format(self.mod_name, self.cls_name)
            prop = getattr(cls, tag, None)

            if not prop:
                module = import_module('.{}'.format(self.mod_name),
                                       'firefox_puppeteer')
                prop = getattr(module, self.cls_name)(cls.marionette)
                setattr(cls, tag, prop)
            func(cls, *args, **kwargs)
            return prop
        return _
