# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import re


def split_symbol(treeherder_symbol):
    """Split a symbol expressed as grp(sym) into its two parts.  If no group is
    given, the returned group is '?'"""
    groupSymbol = '?'
    symbol = treeherder_symbol
    if '(' in symbol:
        groupSymbol, symbol = re.match(r'([^(]*)\(([^)]*)\)', symbol).groups()
    return groupSymbol, symbol


def join_symbol(group, symbol):
    """Perform the reverse of split_symbol, combining the given group and
    symbol.  If the group is '?', then it is omitted."""
    if group == '?':
        return symbol
    return '{}({})'.format(group, symbol)


def add_suffix(treeherder_symbol, suffix):
    """Add a suffix to a treeherder symbol that may contain a group."""
    group, symbol = split_symbol(treeherder_symbol)
    symbol += str(suffix)
    return join_symbol(group, symbol)


def replace_group(treeherder_symbol, new_group):
    """Add a suffix to a treeherder symbol that may contain a group."""
    _, symbol = split_symbol(treeherder_symbol)
    return join_symbol(new_group, symbol)


def inherit_treeherder_from_dep(job, dep_job):
    """Inherit treeherder defaults from dep_job"""
    treeherder = job.get('treeherder', {})

    dep_th_platform = dep_job.task.get('extra', {}).get(
        'treeherder', {}).get('machine', {}).get('platform', '')
    dep_th_collection = list(dep_job.task.get('extra', {}).get(
        'treeherder', {}).get('collection', {}).keys())[0]
    treeherder.setdefault('platform',
                          "{}/{}".format(dep_th_platform, dep_th_collection))
    treeherder.setdefault(
        'tier',
        dep_job.task.get('extra', {}).get('treeherder', {}).get('tier', 1)
    )
    # Does not set symbol
    treeherder.setdefault('kind', 'build')
    return treeherder
