# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:

LINTER = {
    'name': "BadIncludeLinter",
    'description': "Has an invalid include directive.",
    'include': 'should be a list',
    'type': 'string',
    'payload': 'foobar',
}
