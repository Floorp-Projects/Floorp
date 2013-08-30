# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from types import StringTypes
from collections import Iterable


class Makefile(object):
    '''Provides an interface for writing simple makefiles

    Instances of this class are created, populated with rules, then
    written.
    '''

    def __init__(self):
        self._rules = []

    def create_rule(self, targets=[]):
        '''
        Create a new rule in the makefile for the given targets.
        Returns the corresponding Rule instance.
        '''
        rule = Rule(targets)
        self._rules.append(rule)
        return rule

    def dump(self, fh, removal_guard=True):
        '''
        Dump all the rules to the given file handle. Optionally (and by
        default), add guard rules for file removals (empty rules for other
        rules' dependencies)
        '''
        all_deps = set()
        all_targets = set()
        for rule in self._rules:
            rule.dump(fh)
            all_deps.update(rule.dependencies())
            all_targets.update(rule.targets())
        if removal_guard:
            guard = Rule(all_deps - all_targets)
            guard.dump(fh)


class Rule(object):
    '''Class handling simple rules in the form:
           target1 target2 ... : dep1 dep2 ...
                   command1
                   command2
                   ...
    '''
    def __init__(self, targets=[]):
        self._targets = set()
        self._dependencies = set()
        self._commands = []
        self.add_targets(targets)

    def add_targets(self, targets):
        '''Add additional targets to the rule.'''
        assert isinstance(targets, Iterable) and not isinstance(targets, StringTypes)
        self._targets.update(t.replace(os.sep, '/') for t in targets)
        return self

    def add_dependencies(self, deps):
        '''Add dependencies to the rule.'''
        assert isinstance(deps, Iterable) and not isinstance(deps, StringTypes)
        self._dependencies.update(d.replace(os.sep, '/') for d in deps)
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
        return iter(self._dependencies)

    def commands(self):
        '''Return an iterator on the rule commands.'''
        return iter(self._commands)

    def dump(self, fh):
        '''
        Dump the rule to the given file handle.
        '''
        if not self._targets:
            return
        fh.write('%s:' % ' '.join(sorted(self._targets)))
        if self._dependencies:
            fh.write(' %s' % ' '.join(sorted(self._dependencies)))
        fh.write('\n')
        for cmd in self._commands:
            fh.write('\t%s\n' % cmd)
