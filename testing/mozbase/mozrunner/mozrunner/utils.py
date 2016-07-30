#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""Utility functions for mozrunner"""

__all__ = ['findInPath', 'get_metadata_from_egg', 'uses_marionette']


from functools import wraps
import mozinfo
import os
import sys


### python package method metadata by introspection
try:
    import pkg_resources
    def get_metadata_from_egg(module):
        ret = {}
        try:
            dist = pkg_resources.get_distribution(module)
        except pkg_resources.DistributionNotFound:
            return {}
        if dist.has_metadata("PKG-INFO"):
            key = None
            for line in dist.get_metadata("PKG-INFO").splitlines():
                # see http://www.python.org/dev/peps/pep-0314/
                if key == 'Description':
                    # descriptions can be long
                    if not line or line[0].isspace():
                        value += '\n' + line
                        continue
                    else:
                        key = key.strip()
                        value = value.strip()
                        ret[key] = value

                key, value = line.split(':', 1)
                key = key.strip()
                value = value.strip()
                ret[key] = value
        if dist.has_metadata("requires.txt"):
            ret["Dependencies"] = "\n" + dist.get_metadata("requires.txt")
        return ret
except ImportError:
    # package resources not avaialable
    def get_metadata_from_egg(module):
        return {}


def findInPath(fileName, path=os.environ['PATH']):
    """python equivalent of which; should really be in the stdlib"""
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, fileName)):
            return os.path.join(dir, fileName)
        if mozinfo.isWin:
            if os.path.isfile(os.path.join(dir, fileName + ".exe")):
                return os.path.join(dir, fileName + ".exe")

if __name__ == '__main__':
    for i in sys.argv[1:]:
        print findInPath(i)


def _find_marionette_in_args(*args, **kwargs):
    try:
        m = [a for a in args + tuple(kwargs.values()) if hasattr(a, 'session')][0]
    except IndexError:
        print("Can only apply decorator to function using a marionette object")
        raise
    return m


def _raw_log():
    import logging
    return logging.getLogger(__name__)


def test_environment(xrePath, env=None, crashreporter=True, debugger=False,
                     dmdPath=None, lsanPath=None, log=None):
    """
    populate OS environment variables for mochitest and reftests.

    Originally comes from automationutils.py. Don't use that for new code.
    """

    env = os.environ.copy() if env is None else env
    log = log or _raw_log()

    assert os.path.isabs(xrePath)

    if mozinfo.isMac:
        ldLibraryPath = os.path.join(os.path.dirname(xrePath), "MacOS")
    else:
        ldLibraryPath = xrePath

    envVar = None
    dmdLibrary = None
    preloadEnvVar = None
    if 'toolkit' in mozinfo.info and mozinfo.info['toolkit'] == "gonk":
        # Skip all of this, it's only valid for the host.
        pass
    elif mozinfo.isUnix:
        envVar = "LD_LIBRARY_PATH"
        env['MOZILLA_FIVE_HOME'] = xrePath
        dmdLibrary = "libdmd.so"
        preloadEnvVar = "LD_PRELOAD"
    elif mozinfo.isMac:
        envVar = "DYLD_LIBRARY_PATH"
        dmdLibrary = "libdmd.dylib"
        preloadEnvVar = "DYLD_INSERT_LIBRARIES"
    elif mozinfo.isWin:
        envVar = "PATH"
        dmdLibrary = "dmd.dll"
        preloadEnvVar = "MOZ_REPLACE_MALLOC_LIB"
    if envVar:
        envValue = ((env.get(envVar), str(ldLibraryPath))
                    if mozinfo.isWin
                    else (ldLibraryPath, dmdPath, env.get(envVar)))
        env[envVar] = os.path.pathsep.join([path for path in envValue if path])

    if dmdPath and dmdLibrary and preloadEnvVar:
        env[preloadEnvVar] = os.path.join(dmdPath, dmdLibrary)

    # crashreporter
    env['GNOME_DISABLE_CRASH_DIALOG'] = '1'
    env['XRE_NO_WINDOWS_CRASH_DIALOG'] = '1'

    if crashreporter and not debugger:
        env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
        env['MOZ_CRASHREPORTER'] = '1'
    else:
        env['MOZ_CRASHREPORTER_DISABLE'] = '1'

    # Crash on non-local network connections by default.
    # MOZ_DISABLE_NONLOCAL_CONNECTIONS can be set to "0" to temporarily
    # enable non-local connections for the purposes of local testing.  Don't
    # override the user's choice here.  See bug 1049688.
    env.setdefault('MOZ_DISABLE_NONLOCAL_CONNECTIONS', '1')

    # Set WebRTC logging in case it is not set yet
    env.setdefault(
        'MOZ_LOG',
        'signaling:3,mtransport:4,datachannel:4,jsep:4,MediaPipelineFactory:4'
    )
    env.setdefault('R_LOG_LEVEL', '6')
    env.setdefault('R_LOG_DESTINATION', 'stderr')
    env.setdefault('R_LOG_VERBOSE', '1')

    # ASan specific environment stuff
    asan = bool(mozinfo.info.get("asan"))
    if asan and (mozinfo.isLinux or mozinfo.isMac):
        try:
            # Symbolizer support
            llvmsym = os.path.join(xrePath, "llvm-symbolizer")
            if os.path.isfile(llvmsym):
                env["ASAN_SYMBOLIZER_PATH"] = llvmsym
                log.info("INFO | runtests.py | ASan using symbolizer at %s"
                         % llvmsym)
            else:
                log.info("TEST-UNEXPECTED-FAIL | runtests.py | Failed to find"
                         " ASan symbolizer at %s" % llvmsym)

            # Returns total system memory in kilobytes.
            # Works only on unix-like platforms where `free` is in the path.
            totalMemory = int(os.popen("free").readlines()[1].split()[1])

            # Only 4 GB RAM or less available? Use custom ASan options to reduce
            # the amount of resources required to do the tests. Standard options
            # will otherwise lead to OOM conditions on the current test slaves.
            message = "INFO | runtests.py | ASan running in %s configuration"
            asanOptions = []
            if totalMemory <= 1024 * 1024 * 4:
                message = message % 'low-memory'
                asanOptions = [
                    'quarantine_size=50331648', 'malloc_context_size=5']
            else:
                message = message % 'default memory'

            if lsanPath:
                log.info("LSan enabled.")
                asanOptions.append('detect_leaks=1')
                lsanOptions = ["exitcode=0"]
                # Uncomment out the next line to report the addresses of leaked objects.
                #lsanOptions.append("report_objects=1")
                suppressionsFile = os.path.join(
                    lsanPath, 'lsan_suppressions.txt')
                if os.path.exists(suppressionsFile):
                    log.info("LSan using suppression file " + suppressionsFile)
                    lsanOptions.append("suppressions=" + suppressionsFile)
                else:
                    log.info("WARNING | runtests.py | LSan suppressions file"
                             " does not exist! " + suppressionsFile)
                env["LSAN_OPTIONS"] = ':'.join(lsanOptions)

            if len(asanOptions):
                env['ASAN_OPTIONS'] = ':'.join(asanOptions)

        except OSError as err:
            log.info("Failed determine available memory, disabling ASan"
                     " low-memory configuration: %s" % err.strerror)
        except:
            log.info("Failed determine available memory, disabling ASan"
                     " low-memory configuration")
        else:
            log.info(message)

    tsan = bool(mozinfo.info.get("tsan"))
    if tsan and mozinfo.isLinux:
        # Symbolizer support.
        llvmsym = os.path.join(xrePath, "llvm-symbolizer")
        if os.path.isfile(llvmsym):
            env["TSAN_OPTIONS"] = "external_symbolizer_path=%s" % llvmsym
            log.info("INFO | runtests.py | TSan using symbolizer at %s"
                     % llvmsym)
        else:
            log.info("TEST-UNEXPECTED-FAIL | runtests.py | Failed to find TSan"
                     " symbolizer at %s" % llvmsym)

    return env


def get_stack_fixer_function(utilityPath, symbolsPath):
    """
    Return a stack fixing function, if possible, to use on output lines.

    A stack fixing function checks if a line conforms to the output from
    MozFormatCodeAddressDetails.  If the line does not, the line is returned
    unchanged.  If the line does, an attempt is made to convert the
    file+offset into something human-readable (e.g. a function name).
    """
    if not mozinfo.info.get('debug'):
        return None

    stack_fixer_function = None

    def import_stack_fixer_module(module_name):
        sys.path.insert(0, utilityPath)
        module = __import__(module_name, globals(), locals(), [])
        sys.path.pop(0)
        return module

    if symbolsPath and os.path.exists(symbolsPath):
        # Run each line through a function in fix_stack_using_bpsyms.py (uses breakpad symbol files).
        # This method is preferred for Tinderbox builds, since native
        # symbols may have been stripped.
        stack_fixer_module = import_stack_fixer_module(
            'fix_stack_using_bpsyms')
        stack_fixer_function = lambda line: stack_fixer_module.fixSymbols(
            line, symbolsPath)

    elif mozinfo.isMac:
        # Run each line through fix_macosx_stack.py (uses atos).
        # This method is preferred for developer machines, so we don't
        # have to run "make buildsymbols".
        stack_fixer_module = import_stack_fixer_module(
            'fix_macosx_stack')
        stack_fixer_function = lambda line: stack_fixer_module.fixSymbols(
            line)

    elif mozinfo.isLinux:
        # Run each line through fix_linux_stack.py (uses addr2line).
        # This method is preferred for developer machines, so we don't
        # have to run "make buildsymbols".
        stack_fixer_module = import_stack_fixer_module(
            'fix_linux_stack')
        stack_fixer_function = lambda line: stack_fixer_module.fixSymbols(
            line)

    return stack_fixer_function
