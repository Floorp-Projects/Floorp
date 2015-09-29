# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Utility functions for Talos"""

import os
import sys
import time
import urlparse
import string
import urllib
import logging
import json
import re
import platform

# directory of this file for use with interpolatePath()
here = os.path.dirname(os.path.realpath(__file__))


def _get_platform():
    # get the platform we're interested in. Note that the values
    # are used in TTest historically, this is why they are not really friendly.
    # TODO: give some user friendly values
    if platform.system() == "Linux":
        return 'linux_'
    elif platform.system() in ("Windows", "Microsoft"):
        if '5.1' in platform.version():  # winxp
            return 'win_'
        elif '6.1' in platform.version():  # w7
            return 'w7_'
        elif '6.2' in platform.version():  # w8
            return 'w8_'
        else:
            raise TalosError('unsupported windows version')
    elif platform.system() == "Darwin":
        return 'mac_'


PLATFORM_TYPE = _get_platform()


class Timer(object):
    def __init__(self):
        self._start_time = 0
        self.start()

    def start(self):
        self._start_time = time.time()

    def elapsed(self):
        seconds = time.time() - self._start_time
        return time.strftime("%H:%M:%S", time.gmtime(seconds))


def startLogger(levelChoice):
    # declare and define global logger object to send logging messages to
    log_levels = {'debug': logging.DEBUG, 'info': logging.INFO}
    logging.basicConfig(format='%(asctime)-15s %(levelname)s : %(message)s',
                        level=log_levels[levelChoice])


class TalosError(Exception):
    "Errors found while running the talos harness."


class TalosRegression(Exception):
    """When a regression is detected at runtime, report it properly
       Currently this is a simple definition so we can detect the class type
    """


class TalosCrash(Exception):
    """Exception type where we want to report a crash and stay
       compatible with tbpl while allowing us to continue on.

       https://bugzilla.mozilla.org/show_bug.cgi?id=829734
    """


def interpolate(template, **kwargs):
    """
    Use string.Template to substitute variables in a string.

    The placeholder ${talos} is always defined and will be replaced by the
    folder containing this file (global variable 'here').

    You can add placeholders using kwargs.
    """
    kwargs.setdefault('talos', here)
    return string.Template(template).safe_substitute(**kwargs)


def findall(string, token):
    """find all occurences in a string"""
    return [m.start() for m in re.finditer(re.escape(token), string)]


def tokenize(string, start, end):
    """
    tokenize a string by start + end tokens,
    returns parts and position of last token
    """
    assert end not in start, \
        "End token '%s' is contained in start token '%s'" % (end, start)
    assert start not in end, \
        "Start token '%s' is contained in end token '%s'" % (start, end)
    _start = findall(string, start)
    _end = findall(string, end)
    if not _start and not _end:
        return [], -1
    assert len(_start), "Could not find start token: '%s'" % start
    assert len(_end), "Could not find end token: '%s'" % end
    assert len(_start) == len(_end), \
        ("Unmatched number of tokens found: '%s' (%d) vs '%s' (%d)"
         % (start, len(_start), end, len(_end)))
    for i in range(len(_start)):
        assert _end[i] > _start[i], \
            "End token '%s' occurs before start token '%s'" % (end, start)
    parts = []
    for i in range(len(_start)):
        parts.append(string[_start[i] + len(start):_end[i]])
    return parts, _end[-1]


def urlsplit(url, default_scheme='file'):
    """front-end to urlparse.urlsplit"""

    if '://' not in url:
        url = '%s://%s' % (default_scheme, url)

    if url.startswith('file://'):
        # file:// URLs do not play nice with windows
        # https://bugzilla.mozilla.org/show_bug.cgi?id=793875
        return ['file', '', url[len('file://'):], '', '']

    # split the URL and return a list
    return [i for i in urlparse.urlsplit(url)]


def parse_pref(value):
    """parse a preference value from a string"""
    from mozprofile.prefs import Preferences
    return Preferences.cast(value)


def GenerateBrowserCommandLine(browser_path, extra_args, profile_dir,
                               url, profiling_info=None):
    # TODO: allow for spaces in file names on Windows

    command_args = [browser_path.strip()]
    if platform.system() == "Darwin":
        command_args.extend(['-foreground'])

    if isinstance(extra_args, list):
        command_args.extend(extra_args)

    elif extra_args.strip():
        command_args.extend([extra_args])

    command_args.extend(['-profile', profile_dir])

    if profiling_info:
        # For pageloader, buildCommandLine() puts the -tp* command line
        # options into the url argument.
        # It would be better to assemble all -tp arguments in one place,
        # but we don't have the profiling information in buildCommandLine().
        if url.find(' -tp') != -1:
            command_args.extend(['-tpprofilinginfo',
                                 json.dumps(profiling_info)])
        elif url.find('?') != -1:
            url += '&' + urllib.urlencode(profiling_info)
        else:
            url += '?' + urllib.urlencode(profiling_info)

    command_args.extend(url.split(' '))

    # Handle media performance tests
    if url.find('media_manager.py') != -1:
        command_args = url.split(' ')

    logging.debug("command line: %s", ' '.join(command_args))
    return command_args


def indexed_items(itr):
    """
    Generator that allows us to figure out which item is the last one so
    that we can serialize this data properly
    """
    prev_i, prev_val = 0, itr.next()
    for i, val in enumerate(itr, start=1):
        yield prev_i, prev_val
        prev_i, prev_val = i, val
    yield -1, prev_val
