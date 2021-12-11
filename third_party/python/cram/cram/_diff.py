"""Utilities for diffing test files and their output"""

import codecs
import difflib
import re

from cram._encoding import b

__all__ = ['esc', 'glob', 'regex', 'unified_diff']

def _regex(pattern, s):
    """Match a regular expression or return False if invalid.

    >>> from cram._encoding import b
    >>> [bool(_regex(r, b('foobar'))) for r in (b('foo.*'), b('***'))]
    [True, False]
    """
    try:
        return re.match(pattern + b(r'\Z'), s)
    except re.error:
        return False

def _glob(el, l):
    r"""Match a glob-like pattern.

    The only supported special characters are * and ?. Escaping is
    supported.

    >>> from cram._encoding import b
    >>> bool(_glob(b(r'\* \\ \? fo?b*'), b('* \\ ? foobar')))
    True
    """
    i, n = 0, len(el)
    res = b('')
    while i < n:
        c = el[i:i + 1]
        i += 1
        if c == b('\\') and el[i] in b('*?\\'):
            res += el[i - 1:i + 1]
            i += 1
        elif c == b('*'):
            res += b('.*')
        elif c == b('?'):
            res += b('.')
        else:
            res += re.escape(c)
    return _regex(res, l)

def _matchannotation(keyword, matchfunc, el, l):
    """Apply match function based on annotation keyword"""
    ann = b(' (%s)\n' % keyword)
    return el.endswith(ann) and matchfunc(el[:-len(ann)], l[:-1])

def regex(el, l):
    """Apply a regular expression match to a line annotated with '(re)'"""
    return _matchannotation('re', _regex, el, l)

def glob(el, l):
    """Apply a glob match to a line annotated with '(glob)'"""
    return _matchannotation('glob', _glob, el, l)

def esc(el, l):
    """Apply an escape match to a line annotated with '(esc)'"""
    ann = b(' (esc)\n')

    if el.endswith(ann):
        el = codecs.escape_decode(el[:-len(ann)])[0] + b('\n')
    if el == l:
        return True

    if l.endswith(ann):
        l = codecs.escape_decode(l[:-len(ann)])[0] + b('\n')
    return el == l

class _SequenceMatcher(difflib.SequenceMatcher, object):
    """Like difflib.SequenceMatcher, but supports custom match functions"""
    def __init__(self, *args, **kwargs):
        self._matchers = kwargs.pop('matchers', [])
        super(_SequenceMatcher, self).__init__(*args, **kwargs)

    def _match(self, el, l):
        """Tests for matching lines using custom matchers"""
        for matcher in self._matchers:
            if matcher(el, l):
                return True
        return False

    def find_longest_match(self, alo, ahi, blo, bhi):
        """Find longest matching block in a[alo:ahi] and b[blo:bhi]"""
        # SequenceMatcher uses find_longest_match() to slowly whittle down
        # the differences between a and b until it has each matching block.
        # Because of this, we can end up doing the same matches many times.
        matches = []
        for n, (el, line) in enumerate(zip(self.a[alo:ahi], self.b[blo:bhi])):
            if el != line and self._match(el, line):
                # This fools the superclass's method into thinking that the
                # regex/glob in a is identical to b by replacing a's line (the
                # expected output) with b's line (the actual output).
                self.a[alo + n] = line
                matches.append((n, el))
        ret = super(_SequenceMatcher, self).find_longest_match(alo, ahi,
                                                               blo, bhi)
        # Restore the lines replaced above. Otherwise, the diff output
        # would seem to imply that the tests never had any regexes/globs.
        for n, el in matches:
            self.a[alo + n] = el
        return ret

def unified_diff(l1, l2, fromfile=b(''), tofile=b(''), fromfiledate=b(''),
                 tofiledate=b(''), n=3, lineterm=b('\n'), matchers=None):
    r"""Compare two sequences of lines; generate the delta as a unified diff.

    This is like difflib.unified_diff(), but allows custom matchers.

    >>> from cram._encoding import b
    >>> l1 = [b('a\n'), b('? (glob)\n')]
    >>> l2 = [b('a\n'), b('b\n')]
    >>> (list(unified_diff(l1, l2, b('f1'), b('f2'), b('1970-01-01'),
    ...                    b('1970-01-02'))) ==
    ...  [b('--- f1\t1970-01-01\n'), b('+++ f2\t1970-01-02\n'),
    ...   b('@@ -1,2 +1,2 @@\n'), b(' a\n'), b('-? (glob)\n'), b('+b\n')])
    True

    >>> from cram._diff import glob
    >>> list(unified_diff(l1, l2, matchers=[glob]))
    []
    """
    if matchers is None:
        matchers = []
    started = False
    matcher = _SequenceMatcher(None, l1, l2, matchers=matchers)
    for group in matcher.get_grouped_opcodes(n):
        if not started:
            if fromfiledate:
                fromdate = b('\t') + fromfiledate
            else:
                fromdate = b('')
            if tofiledate:
                todate = b('\t') + tofiledate
            else:
                todate = b('')
            yield b('--- ') + fromfile + fromdate + lineterm
            yield b('+++ ') + tofile + todate + lineterm
            started = True
        i1, i2, j1, j2 = group[0][1], group[-1][2], group[0][3], group[-1][4]
        yield (b("@@ -%d,%d +%d,%d @@" % (i1 + 1, i2 - i1, j1 + 1, j2 - j1)) +
               lineterm)
        for tag, i1, i2, j1, j2 in group:
            if tag == 'equal':
                for line in l1[i1:i2]:
                    yield b(' ') + line
                continue
            if tag == 'replace' or tag == 'delete':
                for line in l1[i1:i2]:
                    yield b('-') + line
            if tag == 'replace' or tag == 'insert':
                for line in l2[j1:j2]:
                    yield b('+') + line
