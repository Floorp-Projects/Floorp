# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re
from collections import defaultdict

from fluent.syntax import ast as ftl
from fluent.syntax.serializer import serialize_variant_key

from .base import Checker
from compare_locales import plurals


MSGS = {
    'missing-msg-ref': 'Missing message reference: {ref}',
    'missing-term-ref': 'Missing term reference: {ref}',
    'obsolete-msg-ref': 'Obsolete message reference: {ref}',
    'obsolete-term-ref': 'Obsolete term reference: {ref}',
    'duplicate-attribute': 'Attribute "{name}" is duplicated',
    'missing-value': 'Missing value',
    'obsolete-value': 'Obsolete value',
    'missing-attribute': 'Missing attribute: {name}',
    'obsolete-attribute': 'Obsolete attribute: {name}',
    'duplicate-variant': 'Variant key "{name}" is duplicated',
    'missing-plural': 'Plural categories missing: {categories}',
}


class ReferenceMessageVisitor(ftl.Visitor):
    def __init__(self):
        # References to Messages, their Attributes, and Terms
        # Store reference name and type
        self.entry_refs = defaultdict(dict)
        # The currently active references
        self.refs = {}
        # Start with the Entry value (associated with None)
        self.entry_refs[None] = self.refs
        # If we're a messsage, store if there was a value
        self.message_has_value = False
        # Map attribute names to positions
        self.attribute_positions = {}

    def generic_visit(self, node):
        if isinstance(
            node,
            (ftl.Span, ftl.Annotation, ftl.BaseComment)
        ):
            return
        super(ReferenceMessageVisitor, self).generic_visit(node)

    def visit_Message(self, node):
        if node.value is not None:
            self.message_has_value = True
        super(ReferenceMessageVisitor, self).generic_visit(node)

    def visit_Attribute(self, node):
        self.attribute_positions[node.id.name] = node.span.start
        old_refs = self.refs
        self.refs = self.entry_refs[node.id.name]
        super(ReferenceMessageVisitor, self).generic_visit(node)
        self.refs = old_refs

    def visit_SelectExpression(self, node):
        # optimize select expressions to only go through the variants
        self.visit(node.variants)

    def visit_MessageReference(self, node):
        ref = node.id.name
        if node.attribute:
            ref += '.' + node.attribute.name
        self.refs[ref] = 'msg-ref'

    def visit_TermReference(self, node):
        # only collect term references, but not attributes of terms
        if node.attribute:
            return
        self.refs['-' + node.id.name] = 'term-ref'


class GenericL10nChecks(object):
    '''Helper Mixin for checks shared between Terms and Messages.'''
    def check_duplicate_attributes(self, node):
        warned = set()
        for left in range(len(node.attributes) - 1):
            if left in warned:
                continue
            left_attr = node.attributes[left]
            warned_left = False
            for right in range(left+1, len(node.attributes)):
                right_attr = node.attributes[right]
                if left_attr.id.name == right_attr.id.name:
                    if not warned_left:
                        warned_left = True
                        self.messages.append(
                            (
                                'warning', left_attr.span.start,
                                MSGS['duplicate-attribute'].format(
                                    name=left_attr.id.name
                                )
                            )
                        )
                    warned.add(right)
                    self.messages.append(
                        (
                            'warning', right_attr.span.start,
                            MSGS['duplicate-attribute'].format(
                                name=left_attr.id.name
                            )
                        )
                    )

    def check_variants(self, variants):
        # Check for duplicate variants
        warned = set()
        for left in range(len(variants) - 1):
            if left in warned:
                continue
            left_key = variants[left].key
            key_string = None
            for right in range(left+1, len(variants)):
                if left_key.equals(variants[right].key):
                    if key_string is None:
                        key_string = serialize_variant_key(left_key)
                        self.messages.append(
                            (
                                'warning', left_key.span.start,
                                MSGS['duplicate-variant'].format(
                                    name=key_string
                                )
                            )
                        )
                    warned.add(right)
                    self.messages.append(
                        (
                            'warning', variants[right].key.span.start,
                            MSGS['duplicate-variant'].format(
                                name=key_string
                            )
                        )
                    )
        # Check for plural categories
        if self.locale in plurals.CATEGORIES_BY_LOCALE:
            known_plurals = set(plurals.CATEGORIES_BY_LOCALE[self.locale])
            # Ask for known plurals, but check for plurals w/out `other`.
            # `other` is used for all kinds of things.
            check_plurals = known_plurals.copy()
            check_plurals.discard('other')
            given_plurals = set(serialize_variant_key(v.key) for v in variants)
            if given_plurals & check_plurals:
                missing_plurals = sorted(known_plurals - given_plurals)
                if missing_plurals:
                    self.messages.append(
                        (
                            'warning', variants[0].key.span.start,
                            MSGS['missing-plural'].format(
                                categories=', '.join(missing_plurals)
                            )
                        )
                    )


class L10nMessageVisitor(GenericL10nChecks, ReferenceMessageVisitor):
    def __init__(self, locale, reference):
        super(L10nMessageVisitor, self).__init__()
        self.locale = locale
        # Overload refs to map to sets, just store what we found
        # References to Messages, their Attributes, and Terms
        # Store reference name and type
        self.entry_refs = defaultdict(set)
        # The currently active references
        self.refs = set()
        # Start with the Entry value (associated with None)
        self.entry_refs[None] = self.refs
        self.reference = reference
        self.reference_refs = reference.entry_refs[None]
        self.messages = []

    def visit_Message(self, node):
        self.check_duplicate_attributes(node)
        super(L10nMessageVisitor, self).visit_Message(node)
        if self.message_has_value and not self.reference.message_has_value:
            self.messages.append(
                ('error', node.value.span.start, MSGS['obsolete-value'])
            )
        if not self.message_has_value and self.reference.message_has_value:
            self.messages.append(
                ('error', 0, MSGS['missing-value'])
            )
        ref_attrs = set(self.reference.attribute_positions)
        l10n_attrs = set(self.attribute_positions)
        for missing_attr in ref_attrs - l10n_attrs:
            self.messages.append(
                (
                    'error', 0,
                    MSGS['missing-attribute'].format(name=missing_attr)
                )
            )
        for obs_attr in l10n_attrs - ref_attrs:
            self.messages.append(
                (
                    'error', self.attribute_positions[obs_attr],
                    MSGS['obsolete-attribute'].format(name=obs_attr)
                )
            )

    def visit_Term(self, node):
        raise RuntimeError("Should not use L10nMessageVisitor for Terms")

    def visit_Attribute(self, node):
        old_reference_refs = self.reference_refs
        self.reference_refs = self.reference.entry_refs[node.id.name]
        super(L10nMessageVisitor, self).visit_Attribute(node)
        self.reference_refs = old_reference_refs

    def visit_SelectExpression(self, node):
        super(L10nMessageVisitor, self).visit_SelectExpression(node)
        self.check_variants(node.variants)

    def visit_MessageReference(self, node):
        ref = node.id.name
        if node.attribute:
            ref += '.' + node.attribute.name
        self.refs.add(ref)
        self.check_obsolete_ref(node, ref, 'msg-ref')

    def visit_TermReference(self, node):
        if node.attribute:
            return
        ref = '-' + node.id.name
        self.refs.add(ref)
        self.check_obsolete_ref(node, ref, 'term-ref')

    def check_obsolete_ref(self, node, ref, ref_type):
        if ref not in self.reference_refs:
            self.messages.append(
                (
                    'warning', node.span.start,
                    MSGS['obsolete-' + ref_type].format(ref=ref),
                )
            )


class TermVisitor(GenericL10nChecks, ftl.Visitor):
    def __init__(self, locale):
        super(TermVisitor, self).__init__()
        self.locale = locale
        self.messages = []

    def generic_visit(self, node):
        if isinstance(
            node,
            (ftl.Span, ftl.Annotation, ftl.BaseComment)
        ):
            return
        super(TermVisitor, self).generic_visit(node)

    def visit_Message(self, node):
        raise RuntimeError("Should not use TermVisitor for Messages")

    def visit_Term(self, node):
        self.check_duplicate_attributes(node)
        super(TermVisitor, self).generic_visit(node)

    def visit_SelectExpression(self, node):
        super(TermVisitor, self).generic_visit(node)
        self.check_variants(node.variants)


class FluentChecker(Checker):
    '''Tests to run on Fluent (FTL) files.
    '''
    pattern = re.compile(r'.*\.ftl')

    def check_message(self, ref_entry, l10n_entry):
        '''Run checks on localized messages against reference message.'''
        ref_data = ReferenceMessageVisitor()
        ref_data.visit(ref_entry)
        l10n_data = L10nMessageVisitor(self.locale, ref_data)
        l10n_data.visit(l10n_entry)

        messages = l10n_data.messages
        for attr_or_val, refs in ref_data.entry_refs.items():
            for ref, ref_type in refs.items():
                if ref not in l10n_data.entry_refs[attr_or_val]:
                    msg = MSGS['missing-' + ref_type].format(ref=ref)
                    messages.append(('warning', 0, msg))
        return messages

    def check_term(self, l10n_entry):
        '''Check localized terms.'''
        l10n_data = TermVisitor(self.locale)
        l10n_data.visit(l10n_entry)
        return l10n_data.messages

    def check(self, refEnt, l10nEnt):
        l10n_entry = l10nEnt.entry
        if isinstance(l10n_entry, ftl.Message):
            ref_entry = refEnt.entry
            messages = self.check_message(ref_entry, l10n_entry)
        elif isinstance(l10n_entry, ftl.Term):
            messages = self.check_term(l10n_entry)

        messages.sort(key=lambda t: t[1])
        for cat, pos, msg in messages:
            if pos:
                pos = pos - l10n_entry.span.start
            yield (cat, pos, msg, 'fluent')
