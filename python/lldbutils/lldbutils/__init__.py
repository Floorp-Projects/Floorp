# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import lldb

__all__ = ["content", "general", "gfx", "layout", "utils"]


def init():
    for name in __all__:
        init = None
        try:
            init = __import__("lldbutils." + name, globals(), locals(), ["init"]).init
        except AttributeError:
            pass
        if init:
            init(lldb.debugger)
