# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import re
from types import StringTypes
from collections import Iterable


class Makefile(object):
    '''Provides an interface for writing simple makefiles

    Instances of this class are created, populated with rules, then
    written.
    '''

    def __init__(self):
        self._statements = []

    def create_rule(self, targets=[]):
        '''
        Create a new rule in the makefile for the given targets.
        Returns the corresponding Rule instance.
        '''
        rule = Rule(targets)
        self._statements.append(rule)
        return rule

    def add_statement(self, statement):
        '''
        Add a raw statement in the makefile. Meant to be used for
        simple variable assignments.
        '''
        self._statements.append(statement)

    def dump(self, fh, removal_guard=True):
        '''
        Dump all the rules to the given file handle. Optionally (and by
        default), add guard rules for file removals (empty rules for other
        rules' dependencies)
        '''
        all_deps = set()
        all_targets = set()
        for statement in self._statements:
            if isinstance(statement, Rule):
                statement.dump(fh)
                all_deps.update(statement.dependencies())
                all_targets.update(statement.targets())
            else:
                fh.write('%s\n' % statement)
        if removal_guard:
            guard = Rule(sorted(all_deps - all_targets))
            guard.dump(fh)


class _SimpleOrderedSet(object):
    '''
    Simple ordered set, specialized for used in Rule below only.
    It doesn't expose a complete API, and normalizes path separators
    at insertion.
    '''
    def __init__(self):
        self._list = []
        self._set = set()

    def __nonzero__(self):
        return bool(self._set)

    def __iter__(self):
        return iter(self._list)

    def __contains__(self, key):
        return key in self._set

    def update(self, iterable):
        def _add(iterable):
            emitted = set()
            for i in iterable:
                i = i.replace(os.sep, '/')
                if i not in self._set and i not in emitted:
                    yield i
                    emitted.add(i)
        added = list(_add(iterable))
        self._set.update(added)
        self._list.extend(added)


class Rule(object):
    '''Class handling simple rules in the form:
           target1 target2 ... : dep1 dep2 ...
                   command1
                   command2
                   ...
    '''
    def __init__(self, targets=[]):
        self._targets = _SimpleOrderedSet()
        self._dependencies = _SimpleOrderedSet()
        self._commands = []
        self.add_targets(targets)

    def add_targets(self, targets):
        '''Add additional targets to the rule.'''
        assert isinstance(targets, Iterable) and not isinstance(targets, StringTypes)
        self._targets.update(targets)
        return self

    def add_dependencies(self, deps):
        '''Add dependencies to the rule.'''
        assert isinstance(deps, Iterable) and not isinstance(deps, StringTypes)
        self._dependencies.update(deps)
        return self

    def add_commands(self, commands):
        '''Add commands to the rule.'''
        assert isinstance(commands, Iterable) and not isinstance(commands, StringTypes)
        self._commands.extend(commands)
        return self

    def targets(self):
        '''Return an iterator on the rule targets.'''
        # Ensure the returned iterator is actually just that, an iterator.
        # Avoids caller fiddling with the set itself.
        return iter(self._targets)

    def dependencies(self):
        '''Return an iterator on the rule dependencies.'''
        return iter(d for d in self._dependencies if not d in self._targets)

    def commands(self):
        '''Return an iterator on the rule commands.'''
        return iter(self._commands)

    def dump(self, fh):
        '''
        Dump the rule to the given file handle.
        '''
        if not self._targets:
            return
        fh.write('%s:' % ' '.join(self._targets))
        if self._dependencies:
            fh.write(' %s' % ' '.join(self.dependencies()))
        fh.write('\n')
        for cmd in self._commands:
            fh.write('\t%s\n' % cmd)


# colon followed by anything except a slash (Windows path detection)
_depfilesplitter = re.compile(r':(?![\\/])')


def read_dep_makefile(fh):
    """
    Read the file handler containing a dep makefile (simple makefile only
    containing dependencies) and returns an iterator of the corresponding Rules
    it contains. Ignores removal guard rules.
    """

    rule = ''
    for line in fh.readlines():
        assert not line.startswith('\t')
        line = line.strip()
        if line.endswith('\\'):
            rule += line[:-1]
        else:
            rule += line
            split_rule = _depfilesplitter.split(rule, 1)
            if len(split_rule) > 1 and split_rule[1].strip():
                yield Rule(split_rule[0].strip().split()) \
                      .add_dependencies(split_rule[1].strip().split())
            rule = ''

    if rule:
        raise Exception('Makefile finishes with a backslash. Expected more input.')

def write_dep_makefile(fh, target, deps):
    '''
    Write a Makefile containing only target's dependencies to the file handle
    specified.
    '''
    mk = Makefile()
    rule = mk.create_rule(targets=[target])
    rule.add_dependencies(deps)
    mk.dump(fh, removal_guard=True)
