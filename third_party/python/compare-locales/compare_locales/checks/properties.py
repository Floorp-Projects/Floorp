# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
from __future__ import unicode_literals
import re
from difflib import SequenceMatcher
from six.moves import range
from six.moves import zip

from compare_locales.parser import PropertiesEntity
from compare_locales import plurals
from .base import Checker


class PrintfException(Exception):
    def __init__(self, msg, pos):
        self.pos = pos
        self.msg = msg


class PropertiesChecker(Checker):
    '''Tests to run on .properties files.
    '''
    pattern = re.compile(r'.*\.properties$')
    printf = re.compile(r'%(?P<good>%|'
                        r'(?:(?P<number>[1-9][0-9]*)\$)?'
                        r'(?P<width>\*|[0-9]+)?'
                        r'(?P<prec>\.(?:\*|[0-9]+)?)?'
                        r'(?P<spec>[duxXosScpfg]))?')

    def check(self, refEnt, l10nEnt):
        '''Test for the different variable formats.
        '''
        refValue, l10nValue = refEnt.val, l10nEnt.val
        refSpecs = None
        # check for PluralForm.jsm stuff, should have the docs in the
        # comment
        # That also includes intl.properties' pluralRule, so exclude
        # entities with that key and values with just numbers
        if (refEnt.pre_comment
                and 'Localization_and_Plurals' in refEnt.pre_comment.all
                and refEnt.key != 'pluralRule'
                and not re.match(r'\d+$', refValue)):
            for msg_tuple in self.check_plural(refValue, l10nValue):
                yield msg_tuple
            return
        # check for lost escapes
        raw_val = l10nEnt.raw_val
        for m in PropertiesEntity.escape.finditer(raw_val):
            if m.group('single') and \
               m.group('single') not in PropertiesEntity.known_escapes:
                yield ('warning', m.start(),
                       'unknown escape sequence, \\' + m.group('single'),
                       'escape')
        try:
            refSpecs = self.getPrintfSpecs(refValue)
        except PrintfException:
            refSpecs = []
        if refSpecs:
            for t in self.checkPrintf(refSpecs, l10nValue):
                yield t
            return

    def check_plural(self, refValue, l10nValue):
        '''Check for the stringbundle plurals logic.
        The common variable pattern is #1.
        '''
        if self.locale in plurals.CATEGORIES_BY_LOCALE:
            expected_forms = len(plurals.CATEGORIES_BY_LOCALE[self.locale])
            found_forms = l10nValue.count(';') + 1
            msg = 'expecting {} plurals, found {}'.format(
                expected_forms,
                found_forms
            )
            if expected_forms > found_forms:
                yield ('warning', 0, msg, 'plural')
            if expected_forms < found_forms:
                yield ('warning', 0, msg, 'plural')
        pats = set(int(m.group(1)) for m in re.finditer('#([0-9]+)',
                                                        refValue))
        if len(pats) == 0:
            return
        lpats = set(int(m.group(1)) for m in re.finditer('#([0-9]+)',
                                                         l10nValue))
        if pats - lpats:
            yield ('warning', 0, 'not all variables used in l10n',
                   'plural')
            return
        if lpats - pats:
            yield ('error', 0, 'unreplaced variables in l10n',
                   'plural')

    def checkPrintf(self, refSpecs, l10nValue):
        try:
            l10nSpecs = self.getPrintfSpecs(l10nValue)
        except PrintfException as e:
            yield ('error', e.pos, e.msg, 'printf')
            return
        if refSpecs != l10nSpecs:
            sm = SequenceMatcher()
            sm.set_seqs(refSpecs, l10nSpecs)
            msgs = []
            warn = None
            for action, i1, i2, j1, j2 in sm.get_opcodes():
                if action == 'equal':
                    continue
                if action == 'delete':
                    # missing argument in l10n
                    if i2 == len(refSpecs):
                        # trailing specs missing, that's just a warning
                        warn = ', '.join('trailing argument %d `%s` missing' %
                                         (i+1, refSpecs[i])
                                         for i in range(i1, i2))
                    else:
                        for i in range(i1, i2):
                            msgs.append('argument %d `%s` missing' %
                                        (i+1, refSpecs[i]))
                    continue
                if action == 'insert':
                    # obsolete argument in l10n
                    for i in range(j1, j2):
                        msgs.append('argument %d `%s` obsolete' %
                                    (i+1, l10nSpecs[i]))
                    continue
                if action == 'replace':
                    for i, j in zip(range(i1, i2), range(j1, j2)):
                        msgs.append('argument %d `%s` should be `%s`' %
                                    (j+1, l10nSpecs[j], refSpecs[i]))
            if msgs:
                yield ('error', 0, ', '.join(msgs), 'printf')
            if warn is not None:
                yield ('warning', 0, warn, 'printf')

    def getPrintfSpecs(self, val):
        hasNumber = False
        specs = []
        for m in self.printf.finditer(val):
            if m.group("good") is None:
                # found just a '%', signal an error
                raise PrintfException('Found single %', m.start())
            if m.group("good") == '%':
                # escaped %
                continue
            if ((hasNumber and m.group('number') is None) or
                    (not hasNumber and specs and
                     m.group('number') is not None)):
                # mixed style, numbered and not
                raise PrintfException('Mixed ordered and non-ordered args',
                                      m.start())
            hasNumber = m.group('number') is not None
            if hasNumber:
                pos = int(m.group('number')) - 1
                ls = len(specs)
                if pos >= ls:
                    # pad specs
                    nones = pos - ls
                    specs[ls:pos] = nones*[None]
                    specs.append(m.group('spec'))
                else:
                    specs[pos] = m.group('spec')
            else:
                specs.append(m.group('spec'))
        # check for missing args
        if hasNumber and not all(specs):
            raise PrintfException('Ordered argument missing', 0)
        return specs
