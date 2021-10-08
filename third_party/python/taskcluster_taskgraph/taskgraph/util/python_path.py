# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


def find_object(path):
    """
    Find a Python object given a path of the form <modulepath>:<objectpath>.
    Conceptually equivalent to

        def find_object(modulepath, objectpath):
            import <modulepath> as mod
            return mod.<objectpath>
    """
    if path.count(":") != 1:
        raise ValueError(f'python path {path!r} does not have the form "module:object"')

    modulepath, objectpath = path.split(":")
    obj = __import__(modulepath)
    for a in modulepath.split(".")[1:]:
        obj = getattr(obj, a)
    for a in objectpath.split("."):
        obj = getattr(obj, a)
    return obj
