# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import re


def _tokens2re(**tokens):
    # Create a pattern for non-escaped tokens, in the form:
    #   (?<!\\)(?:a|b|c...)
    # This is meant to match patterns a, b, or c, or ... if they are not
    # preceded by a backslash.
    # where a, b, c... are in the form
    #   (?P<name>pattern)
    # which matches the pattern and captures it in a named match group.
    # The group names and patterns are given as arguments.
    all_tokens = '|'.join('(?P<%s>%s)' % (name, value)
                          for name, value in tokens.iteritems())
    nonescaped = r'(?<!\\)(?:%s)' % all_tokens

    # The final pattern matches either the above pattern, or an escaped
    # backslash, captured in the "escape" match group.
    return re.compile('(?:%s|%s)' % (nonescaped, r'(?P<escape>\\\\)'))

UNQUOTED_TOKENS_RE = _tokens2re(
  whitespace=r'[\t\r\n ]+',
  quote=r'[\'"]',
  comment='#',
  special=r'[<>&|`~(){}$;\*\?]',
  backslashed=r'\\[^\\]',
)

DOUBLY_QUOTED_TOKENS_RE = _tokens2re(
  quote='"',
  backslashedquote=r'\\"',
  special='\$',
  backslashed=r'\\[^\\"]',
)

ESCAPED_NEWLINES_RE = re.compile(r'\\\n')

# This regexp contains the same characters as all those listed in
# UNQUOTED_TOKENS_RE. Please keep in sync.
SHELL_QUOTE_RE = re.compile('[\\\t\r\n \'\"#<>&|`~(){}$;\*\?]')


class MetaCharacterException(Exception):
    def __init__(self, char):
        self.char = char


class _ClineSplitter(object):
    '''
    Parses a given command line string and creates a list of command
    and arguments, with wildcard expansion.
    '''
    def __init__(self, cline):
        self.arg = None
        self.cline = cline
        self.result = []
        self._parse_unquoted()

    def _push(self, str):
        '''
        Push the given string as part of the current argument
        '''
        if self.arg is None:
            self.arg = ''
        self.arg += str

    def _next(self):
        '''
        Finalize current argument, effectively adding it to the list.
        '''
        if self.arg is None:
            return
        self.result.append(self.arg)
        self.arg = None

    def _parse_unquoted(self):
        '''
        Parse command line remainder in the context of an unquoted string.
        '''
        while self.cline:
            # Find the next token
            m = UNQUOTED_TOKENS_RE.search(self.cline)
            # If we find none, the remainder of the string can be pushed to
            # the current argument and the argument finalized
            if not m:
                self._push(self.cline)
                break
            # The beginning of the string, up to the found token, is part of
            # the current argument
            if m.start():
                self._push(self.cline[:m.start()])
            self.cline = self.cline[m.end():]

            match = {name: value
                     for name, value in m.groupdict().items() if value}
            if 'quote' in match:
                # " or ' start a quoted string
                if match['quote'] == '"':
                    self._parse_doubly_quoted()
                else:
                    self._parse_quoted()
            elif 'comment' in match:
                # Comments are ignored. The current argument can be finalized,
                # and parsing stopped.
                break
            elif 'special' in match:
                # Unquoted, non-escaped special characters need to be sent to a
                # shell.
                raise MetaCharacterException(match['special'])
            elif 'whitespace' in match:
                # Whitespaces terminate current argument.
                self._next()
            elif 'escape' in match:
                # Escaped backslashes turn into a single backslash
                self._push('\\')
            elif 'backslashed' in match:
                # Backslashed characters are unbackslashed
                # e.g. echo \a -> a
                self._push(match['backslashed'][1])
            else:
                raise Exception("Shouldn't reach here")
        if self.arg:
            self._next()

    def _parse_quoted(self):
        # Single quoted strings are preserved, except for the final quote
        index = self.cline.find("'")
        if index == -1:
            raise Exception('Unterminated quoted string in command')
        self._push(self.cline[:index])
        self.cline = self.cline[index+1:]

    def _parse_doubly_quoted(self):
        if not self.cline:
            raise Exception('Unterminated quoted string in command')
        while self.cline:
            m = DOUBLY_QUOTED_TOKENS_RE.search(self.cline)
            if not m:
                raise Exception('Unterminated quoted string in command')
            self._push(self.cline[:m.start()])
            self.cline = self.cline[m.end():]
            match = {name: value
                     for name, value in m.groupdict().items() if value}
            if 'quote' in match:
                # a double quote ends the quoted string, so go back to
                # unquoted parsing
                return
            elif 'special' in match:
                # Unquoted, non-escaped special characters in a doubly quoted
                # string still have a special meaning and need to be sent to a
                # shell.
                raise MetaCharacterException(match['special'])
            elif 'escape' in match:
                # Escaped backslashes turn into a single backslash
                self._push('\\')
            elif 'backslashedquote' in match:
                # Backslashed double quotes are un-backslashed
                self._push('"')
            elif 'backslashed' in match:
                # Backslashed characters are kept backslashed
                self._push(match['backslashed'])


def split(cline):
    '''
    Split the given command line string.
    '''
    s = ESCAPED_NEWLINES_RE.sub('', cline)
    return _ClineSplitter(s).result


def _quote(s):
    '''Given a string, returns a version that can be used literally on a shell
    command line, enclosing it with single quotes if necessary.

    As a special case, if given an int, returns a string containing the int,
    not enclosed in quotes.
    '''
    if type(s) == int:
        return '%d' % s

    # Empty strings need to be quoted to have any significance
    if s and not SHELL_QUOTE_RE.search(s):
        return s

    # Single quoted strings can contain any characters unescaped except the
    # single quote itself, which can't even be escaped, so the string needs to
    # be closed, an escaped single quote added, and reopened.
    t = type(s)
    return t("'%s'") % s.replace(t("'"), t("'\\''"))


def quote(*strings):
    '''Given one or more strings, returns a quoted string that can be used
    literally on a shell command line.

        >>> quote('a', 'b')
        "a b"
        >>> quote('a b', 'c')
        "'a b' c"
    '''
    return ' '.join(_quote(s) for s in strings)


__all__ = ['MetaCharacterException', 'split', 'quote']
