# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from errors import MarionetteException, TimeoutException
from functools import wraps
import socket
import sys
import traceback


def _find_marionette_in_args(*args, **kwargs):
    try:
        m = [a for a in args + tuple(kwargs.values()) if hasattr(a, 'session')][0]
    except IndexError:
        print("Can only apply decorator to function using a marionette object")
        raise
    return m


def do_process_check(func, always=False):
    """Decorator which checks the process after the function has run.

    There is a check for crashes which always gets executed. And in the case of
    connection issues the process will be force closed.

    :param always: If False, only checks for crashes if an exception
                   was raised. If True, always checks for crashes.
    """
    @wraps(func)
    def _(*args, **kwargs):
        m = _find_marionette_in_args(*args, **kwargs)

        def check_for_crash():
            try:
                m.check_for_crash()
            except:
                # don't want to lose the original exception
                traceback.print_exc()

        try:
            return func(*args, **kwargs)
        except (MarionetteException, socket.error, IOError) as e:
            exc, val, tb = sys.exc_info()
            if not isinstance(e, MarionetteException) or type(e) is MarionetteException:
                if not always:
                    check_for_crash()
            if not isinstance(e, MarionetteException) or type(e) is TimeoutException:
                m.force_shutdown()
            raise exc, val, tb
        finally:
            if always:
                check_for_crash(m)
    return _


def uses_marionette(func):
    """Decorator which creates a marionette session and deletes it
    afterwards if one doesn't already exist.
    """
    @wraps(func)
    def _(*args, **kwargs):
        m = _find_marionette_in_args(*args, **kwargs)
        delete_session = False
        if not m.session:
            delete_session = True
            m.start_session()

        m.set_context(m.CONTEXT_CHROME)
        ret = func(*args, **kwargs)

        if delete_session:
            m.delete_session()

        return ret
    return _


def using_context(context):
    """Decorator which allows a function to execute in certain scope
    using marionette.using_context functionality and returns to old
    scope once the function exits.
    :param context: Either 'chrome' or 'content'.
    """
    def wrap(func):
        @wraps(func)
        def inner(*args, **kwargs):
            m = _find_marionette_in_args(*args, **kwargs)
            with m.using_context(context):
                return func(*args, **kwargs)

        return inner
    return wrap
