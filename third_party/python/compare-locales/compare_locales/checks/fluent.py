# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re
from collections import Counter

from fluent.syntax import ast as ftl

from compare_locales.parser import FluentMessage
from .base import Checker


class FluentChecker(Checker):
    '''Tests to run on Fluent (FTL) files.
    '''
    pattern = re.compile(r'.*\.ftl')

    def find_message_references(self, entry):
        refs = {}

        def collect_message_references(node):
            if isinstance(node, ftl.MessageReference):
                # The key is the name of the referenced message and it will
                # be used in set algebra to find missing and obsolete
                # references. The value is the node itself and its span
                # will be used to pinpoint the error.
                refs[node.id.name] = node
            if isinstance(node, ftl.TermReference):
                # Same for terms, store them as -term.id
                refs['-' + node.id.name] = node
            # BaseNode.traverse expects this function to return the node.
            return node

        entry.traverse(collect_message_references)
        return refs

    def check_values(self, ref_entry, l10n_entry):
        '''Verify that values match, either both have a value or none.'''
        if ref_entry.value is not None and l10n_entry.value is None:
            yield ('error', 0, 'Missing value', 'fluent')
        if ref_entry.value is None and l10n_entry.value is not None:
            offset = l10n_entry.value.span.start - l10n_entry.span.start
            yield ('error', offset, 'Obsolete value', 'fluent')

    def check_message_references(self, ref_entry, l10n_entry):
        '''Verify that message references are the same.'''
        ref_msg_refs = self.find_message_references(ref_entry)
        l10n_msg_refs = self.find_message_references(l10n_entry)

        # create unique sets of message names referenced in both entries
        ref_msg_refs_names = set(ref_msg_refs.keys())
        l10n_msg_refs_names = set(l10n_msg_refs.keys())

        missing_msg_ref_names = ref_msg_refs_names - l10n_msg_refs_names
        for msg_name in missing_msg_ref_names:
            yield ('warning', 0, 'Missing message reference: ' + msg_name,
                   'fluent')

        obsolete_msg_ref_names = l10n_msg_refs_names - ref_msg_refs_names
        for msg_name in obsolete_msg_ref_names:
            pos = l10n_msg_refs[msg_name].span.start - l10n_entry.span.start
            yield ('warning', pos, 'Obsolete message reference: ' + msg_name,
                   'fluent')

    def check_attributes(self, ref_entry, l10n_entry):
        '''Verify that ref_entry and l10n_entry have the same attributes.'''
        ref_attr_names = set((attr.id.name for attr in ref_entry.attributes))
        ref_pos = dict((attr.id.name, i)
                       for i, attr in enumerate(ref_entry.attributes))
        l10n_attr_counts = \
            Counter(attr.id.name for attr in l10n_entry.attributes)
        l10n_attr_names = set(l10n_attr_counts)
        l10n_pos = dict((attr.id.name, i)
                        for i, attr in enumerate(l10n_entry.attributes))
        # check for duplicate Attributes
        # only warn to not trigger a merge skip
        for attr_name, cnt in l10n_attr_counts.items():
            if cnt > 1:
                offset = (
                    l10n_entry.attributes[l10n_pos[attr_name]].span.start
                    - l10n_entry.span.start)
                yield (
                    'warning',
                    offset,
                    'Attribute "{}" occurs {} times'.format(
                        attr_name, cnt),
                    'fluent')

        missing_attr_names = sorted(ref_attr_names - l10n_attr_names,
                                    key=lambda k: ref_pos[k])
        for attr_name in missing_attr_names:
            yield ('error', 0, 'Missing attribute: ' + attr_name, 'fluent')

        obsolete_attr_names = sorted(l10n_attr_names - ref_attr_names,
                                     key=lambda k: l10n_pos[k])
        obsolete_attrs = [
            attr
            for attr in l10n_entry.attributes
            if attr.id.name in obsolete_attr_names
        ]
        for attr in obsolete_attrs:
            yield ('error', attr.span.start - l10n_entry.span.start,
                   'Obsolete attribute: ' + attr.id.name, 'fluent')

    def check(self, refEnt, l10nEnt):
        ref_entry = refEnt.entry
        l10n_entry = l10nEnt.entry

        # PY3 Replace with `yield from` in Python 3.3+
        for check in self.check_values(ref_entry, l10n_entry):
            yield check

        for check in self.check_message_references(ref_entry, l10n_entry):
            yield check

        # Only compare attributes of Fluent Messages. Attributes defined on
        # Fluent Terms are private.
        if isinstance(refEnt, FluentMessage):
            for check in self.check_attributes(ref_entry, l10n_entry):
                yield check
