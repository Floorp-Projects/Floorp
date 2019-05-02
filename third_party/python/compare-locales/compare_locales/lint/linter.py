# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import
from __future__ import unicode_literals

from collections import Counter
import os

from compare_locales import parser, checks
from compare_locales.paths import File, REFERENCE_LOCALE


class L10nLinter(object):

    def lint(self, files, get_reference_and_tests):
        results = []
        for path in files:
            if not parser.hasParser(path):
                continue
            ref, extra_tests = get_reference_and_tests(path)
            results.extend(self.lint_file(path, ref, extra_tests))
        return results

    def lint_file(self, path, ref, extra_tests):
        file_parser = parser.getParser(path)
        if os.path.isfile(ref):
            file_parser.readFile(ref)
            reference = file_parser.parse()
        else:
            reference = {}
        file_parser.readFile(path)
        current = file_parser.parse()
        checker = checks.getChecker(
            File(path, path, locale=REFERENCE_LOCALE),
            extra_tests=extra_tests
        )
        if checker and checker.needs_reference:
            checker.set_reference(current)
        linter = EntityLinter(current, checker, reference)
        for current_entity in current:
            for result in linter.lint_entity(current_entity):
                result['path'] = path
                yield result


class EntityLinter(object):
    '''Factored out helper to run linters on a single entity.'''
    def __init__(self, current, checker, reference):
        self.key_count = Counter(entity.key for entity in current)
        self.checker = checker
        self.reference = reference

    def lint_entity(self, current_entity):
        res = self.handle_junk(current_entity)
        if res:
            yield res
            return
        for res in self.lint_full_entity(current_entity):
            yield res
        for res in self.lint_value(current_entity):
            yield res

    def lint_full_entity(self, current_entity):
        '''Checks that go good or bad for a full entity,
        without a particular spot inside the entity.
        '''
        lineno = col = None
        if self.key_count[current_entity.key] > 1:
            lineno, col = current_entity.position()
            yield {
                'lineno': lineno,
                'column': col,
                'level': 'error',
                'message': 'Duplicate string with ID: {}'.format(
                    current_entity.key
                )
            }

        if current_entity.key in self.reference:
            reference_entity = self.reference[current_entity.key]
            if not current_entity.equals(reference_entity):
                if lineno is None:
                    lineno, col = current_entity.position()
                msg = 'Changes to string require a new ID: {}'.format(
                    current_entity.key
                )
                yield {
                    'lineno': lineno,
                    'column': col,
                    'level': 'warning',
                    'message': msg,
                }

    def lint_value(self, current_entity):
        '''Checks that error on particular locations in the entity value.
        '''
        if self.checker:
            for tp, pos, msg, cat in self.checker.check(
                current_entity, current_entity
            ):
                lineno, col = current_entity.value_position(pos)
                yield {
                    'lineno': lineno,
                    'column': col,
                    'level': tp,
                    'message': msg,
                }

    def handle_junk(self, current_entity):
        if not isinstance(current_entity, parser.Junk):
            return None

        lineno, col = current_entity.position()
        return {
            'lineno': lineno,
            'column': col,
            'level': 'error',
            'message': current_entity.error_message()
        }
