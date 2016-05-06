'''
Mercurial extension to enable purging long filenames on Windows

It's possible to have filenaems that exceed the MAX_PATH limit (256 characters)
on Windows, rendering them un-purgeable with the regular python API calls.

To work around this limitation, we can use the DeleteFileW API
(https://msdn.microsoft.com/en-us/library/windows/desktop/aa363915%28v=vs.85%29.aspx)
and prefix the filename with \\?\.

This extension needs to monkeypatch other modules in order to function. It
attempts to be very conservative, and only applies the patches for the
duration of the purge() command. The original functions are restored after
the purge() command exits.
'''
from contextlib import contextmanager
from functools import partial
import os
import errno

import mercurial.extensions
import mercurial.util

testedwith = '3.7'
minimumhgversion = '3.7'

if os.name == 'nt':
    import ctypes

    # Get a reference to the DeleteFileW function
    # DeleteFileW accepts filenames encoded as a null terminated sequence of
    # wide chars (UTF-16). Python's ctypes.c_wchar_p correctly encodes unicode
    # strings to null terminated UTF-16 strings.
    # However, we receive (byte) strings from mercurial. When these are passed
    # to DeleteFileW via the c_wchar_p type, they are implicitly decoded via
    # the 'mbcs' encoding on windows.
    kernel32 = ctypes.windll.kernel32
    DeleteFile = kernel32.DeleteFileW
    DeleteFile.argtypes = [ctypes.c_wchar_p]
    DeleteFile.restype = ctypes.c_bool

    def unlink_long(fn):
        normalized_path = '\\\\?\\' + os.path.normpath(fn)
        if not DeleteFile(normalized_path):
            raise OSError(errno.EPERM, "couldn't remove long path", fn)

# Not needed on other platforms, but is handy for testing
else:
    def unlink_long(fn):
        os.unlink(fn)


def unlink_wrapper(unlink_orig, fn, ui):
    '''Calls the original unlink function, and if that fails, calls
    unlink_long'''
    try:
        ui.debug('calling unlink_orig %s\n' % fn)
        return unlink_orig(fn)
    except WindowsError as e:
        # windows error 3 corresponds to ERROR_PATH_NOT_FOUND
        # only handle this case; re-raise the exception for other kinds of
        # failures
        if e.winerror != 3:
            raise
        ui.debug('caught WindowsError ERROR_PATH_NOT_FOUND; '
                 'calling unlink_long %s\n' % fn)
        return unlink_long(fn)


@contextmanager
def wrap_unlink(ui):
    '''Context manager that patches the required functions that are used by
    the purge extension to remove files. When exiting the context manager
    the original functions are restored.'''
    purgemod = mercurial.extensions.find('purge')
    to_wrap = [(purgemod.util, 'unlink')]

    # pass along the ui object to the unlink_wrapper so we can get logging out
    # of it
    wrapped = partial(unlink_wrapper, ui=ui)

    # Wrap the original function(s) with our unlink_wrapper
    originals = {}
    for mod, func in to_wrap:
        ui.debug('wrapping %s %s\n' % (mod, func))
        originals[mod, func] = mercurial.extensions.wrapfunction(
            mod, func, wrapped)

    try:
        yield
    finally:
        # Restore the originals
        for mod, func in to_wrap:
            ui.debug('restoring %s %s\n' % (mod, func))
            setattr(mod, func, originals[mod, func])


def purge_wrapper(orig, ui, *args, **kwargs):
    '''Runs the original purge() command inside the wrap_unlink() context
    manager.'''
    with wrap_unlink(ui):
        return orig(ui, *args, **kwargs)


def extsetup(ui):
    try:
        purgemod = mercurial.extensions.find('purge')
    except KeyError:
        ui.warn('purge extension not found; '
                'not enabling long filename support\n')
        return

    ui.note('enabling long filename support for purge\n')
    mercurial.extensions.wrapcommand(purgemod.cmdtable, 'purge', purge_wrapper)
