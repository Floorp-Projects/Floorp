from __future__ import unicode_literals


class ParseError(Exception):
    def __init__(self, code, *args):
        self.code = code
        self.args = args
        self.message = get_error_message(code, args)


def get_error_message(code, args):
    if code == 'E00001':
        return 'Generic error'
    if code == 'E0002':
        return 'Expected an entry start'
    if code == 'E0003':
        return 'Expected token: "{}"'.format(args[0])
    if code == 'E0004':
        return 'Expected a character from range: "{}"'.format(args[0])
    if code == 'E0005':
        msg = 'Expected message "{}" to have a value or attributes'
        return msg.format(args[0])
    if code == 'E0006':
        msg = 'Expected term "-{}" to have a value'
        return msg.format(args[0])
    if code == 'E0007':
        return 'Keyword cannot end with a whitespace'
    if code == 'E0008':
        return 'The callee has to be an upper-case identifier or a term'
    if code == 'E0009':
        return 'The argument name has to be a simple identifier'
    if code == 'E0010':
        return 'Expected one of the variants to be marked as default (*)'
    if code == 'E0011':
        return 'Expected at least one variant after "->"'
    if code == 'E0012':
        return 'Expected value'
    if code == 'E0013':
        return 'Expected variant key'
    if code == 'E0014':
        return 'Expected literal'
    if code == 'E0015':
        return 'Only one variant can be marked as default (*)'
    if code == 'E0016':
        return 'Message references cannot be used as selectors'
    if code == 'E0017':
        return 'Terms cannot be used as selectors'
    if code == 'E0018':
        return 'Attributes of messages cannot be used as selectors'
    if code == 'E0019':
        return 'Attributes of terms cannot be used as placeables'
    if code == 'E0020':
        return 'Unterminated string expression'
    if code == 'E0021':
        return 'Positional arguments must not follow named arguments'
    if code == 'E0022':
        return 'Named arguments must be unique'
    if code == 'E0024':
        return 'Cannot access variants of a message.'
    if code == 'E0025':
        return 'Unknown escape sequence: \\{}.'.format(args[0])
    if code == 'E0026':
        return 'Invalid Unicode escape sequence: {}.'.format(args[0])
    if code == 'E0027':
        return 'Unbalanced closing brace in TextElement.'
    if code == 'E0028':
        return 'Expected an inline expression'
    return code
