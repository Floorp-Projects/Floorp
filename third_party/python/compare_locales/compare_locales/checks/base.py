# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re


class EntityPos(int):
    pass


mochibake = re.compile('\ufffd')


class Checker:
    '''Abstract class to implement checks per file type.
    '''
    pattern = None
    # if a check uses all reference entities, set this to True
    needs_reference = False

    @classmethod
    def use(cls, file):
        return cls.pattern.match(file.file)

    def __init__(self, extra_tests, locale=None):
        self.extra_tests = extra_tests
        self.locale = locale
        self.reference = None

    def check(self, refEnt, l10nEnt):
        '''Given the reference and localized Entities, performs checks.

        This is a generator yielding tuples of
        - "warning" or "error", depending on what should be reported,
        - tuple of line, column info for the error within the string
        - description string to be shown in the report

        By default, check for possible encoding errors.
        '''
        for m in mochibake.finditer(l10nEnt.all):
            yield (
                "warning",
                EntityPos(m.start()),
                f"\ufffd in: {l10nEnt.key}",
                "encodings"
            )

    def set_reference(self, reference):
        '''Set the reference entities.
        Only do this if self.needs_reference is True.
        '''
        self.reference = reference


class CSSCheckMixin:
    def maybe_style(self, ref_value, l10n_value):
        ref_map, _ = self.parse_css_spec(ref_value)
        if not ref_map:
            return
        l10n_map, errors = self.parse_css_spec(l10n_value)
        yield from self.check_style(ref_map, l10n_map, errors)

    def check_style(self, ref_map, l10n_map, errors):
        if not l10n_map:
            yield ('error', 0, 'reference is a CSS spec', 'css')
            return
        if errors:
            yield ('error', 0, 'reference is a CSS spec', 'css')
            return
        msgs = []
        for prop, unit in l10n_map.items():
            if prop not in ref_map:
                msgs.insert(0, '%s only in l10n' % prop)
                continue
            else:
                ref_unit = ref_map.pop(prop)
                if unit != ref_unit:
                    msgs.append("units for %s don't match "
                                "(%s != %s)" % (prop, unit, ref_unit))
        for prop in ref_map.keys():
            msgs.insert(0, '%s only in reference' % prop)
        if msgs:
            yield ('warning', 0, ', '.join(msgs), 'css')

    def parse_css_spec(self, val):
        if not hasattr(self, '_css_spec'):
            self._css_spec = re.compile(
                r'(?:'
                r'(?P<prop>(?:min\-|max\-)?(?:width|height))'
                r'[ \t\r\n]*:[ \t\r\n]*'
                r'(?P<length>[0-9]+|[0-9]*\.[0-9]+)'
                r'(?P<unit>ch|em|ex|rem|px|cm|mm|in|pc|pt)'
                r')'
                r'|\Z'
            )
            self._css_sep = re.compile(r'[ \t\r\n]*(?P<semi>;)?[ \t\r\n]*$')
        refMap = errors = None
        end = 0
        for m in self._css_spec.finditer(val):
            if end == 0 and m.start() == m.end():
                # no CSS spec found, just immediately end of string
                return None, None
            if m.start() > end:
                split = self._css_sep.match(val, end, m.start())
                if split is None:
                    errors = errors or []
                    errors.append({
                        'pos': end,
                        'code': 'css-bad-content',
                    })
                elif end > 0 and split.group('semi') is None:
                    errors = errors or []
                    errors.append({
                        'pos': end,
                        'code': 'css-missing-semicolon',
                    })
            if m.group('prop'):
                refMap = refMap or {}
                refMap[m.group('prop')] = m.group('unit')
            end = m.end()
        return refMap, errors
