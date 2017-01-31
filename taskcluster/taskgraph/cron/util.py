# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from __future__ import absolute_import, print_function, unicode_literals

import subprocess


def match_utc(params, hour=None, minute=None):
    """ Return True if params['time'] matches the given hour and minute.
    If hour is not specified, any hour will match.  If minute is not
    specified, then every multiple of fifteen minutes will match.  Times
    not an even multiple of fifteen minutes will result in an exception
    (since they would never run)."""
    if minute and minute % 15 != 0:
        raise Exception("cron jobs only run on multiples of 15 minutes past the hour")
    if hour and params['time'].hour != hour:
        return False
    if minute and params['time'].minute != minute:
        return False
    return True


def calculate_head_rev(options):
    # we assume that run-task has correctly checked out the revision indicated by
    # GECKO_HEAD_REF, so all that remains is to see what the current revision is.
    # Mercurial refers to that as `.`.
    return subprocess.check_output(['hg', 'log', '-r', '.', '-T', '{node}'])
