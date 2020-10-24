# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import inspect
import os


def find_object(path):
    """
    Find a Python object given a path of the form <modulepath>:<objectpath>.
    Conceptually equivalent to

        def find_object(modulepath, objectpath):
            import <modulepath> as mod
            return mod.<objectpath>
    """
    if path.count(':') != 1:
        raise ValueError(
            'python path {!r} does not have the form "module:object"'.format(path))

    modulepath, objectpath = path.split(':')
    obj = __import__(modulepath)
    for a in modulepath.split('.')[1:]:
        obj = getattr(obj, a)
    for a in objectpath.split('.'):
        obj = getattr(obj, a)
    return obj


def import_sibling_modules(exceptions=None):
    """
    Import all Python modules that are siblings of the calling module.

    Args:
        exceptions (list): A list of file names to exclude (caller and
            __init__.py are implicitly excluded).
    """
    frame = inspect.stack()[1]
    mod = inspect.getmodule(frame[0])

    name = os.path.basename(mod.__file__)
    excs = set(['__init__.py', name])
    if exceptions:
        excs.update(exceptions)

    modpath = mod.__name__
    if not name.startswith('__init__.py'):
        modpath = modpath.rsplit('.', 1)[0]

    for f in os.listdir(os.path.dirname(mod.__file__)):
        if f.endswith('.py') and f not in excs:
            __import__(modpath + '.' + f[:-3])
