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
        msg = 'Expected entry "{}" to have a value or attributes'
        return msg.format(args[0])
    if code == 'E0006':
        return 'Expected field: "{}"'.format(args[0])
    if code == 'E0007':
        return 'Keyword cannot end with a whitespace'
    if code == 'E0008':
        return 'Callee has to be a simple identifier'
    if code == 'E0009':
        return 'Key has to be a simple identifier'
    if code == 'E0010':
        return 'Expected one of the variants to be marked as default (*)'
    if code == 'E0011':
        return 'Expected at least one variant after "->"'
    if code == 'E0012':
        return 'Tags cannot be added to messages with attributes'
    if code == 'E0013':
        return 'Expected variant key'
    if code == 'E0014':
        return 'Expected literal'
    if code == 'E0015':
        return 'Only one variant can be marked as default (*)'
    return code
