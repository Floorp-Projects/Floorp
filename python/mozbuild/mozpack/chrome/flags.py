# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
from distutils.version import LooseVersion
from mozpack.errors import errors
from collections import OrderedDict


class Flag(object):
    '''
    Class for flags in manifest entries in the form:
        "flag"   (same as "flag=true")
        "flag=yes|true|1"
        "flag=no|false|0"
    '''
    def __init__(self, name):
        '''
        Initialize a Flag with the given name.
        '''
        self.name = name
        self.value = None

    def add_definition(self, definition):
        '''
        Add a flag value definition. Replaces any previously set value.
        '''
        if definition == self.name:
            self.value = True
            return
        assert(definition.startswith(self.name))
        if definition[len(self.name)] != '=':
            return errors.fatal('Malformed flag: %s' % definition)
        value = definition[len(self.name) + 1:]
        if value in ('yes', 'true', '1', 'no', 'false', '0'):
            self.value = value
        else:
            return errors.fatal('Unknown value in: %s' % definition)

    def matches(self, value):
        '''
        Return whether the flag value matches the given value. The values
        are canonicalized for comparison.
        '''
        if value in ('yes', 'true', '1', True):
            return self.value in ('yes', 'true', '1', True)
        if value in ('no', 'false', '0', False):
            return self.value in ('no', 'false', '0', False, None)
        raise RuntimeError('Invalid value: %s' % value)

    def __str__(self):
        '''
        Serialize the flag value in the same form given to the last
        add_definition() call.
        '''
        if self.value is None:
            return ''
        if self.value is True:
            return self.name
        return '%s=%s' % (self.name, self.value)


class StringFlag(object):
    '''
    Class for string flags in manifest entries in the form:
        "flag=string"
        "flag!=string"
    '''
    def __init__(self, name):
        '''
        Initialize a StringFlag with the given name.
        '''
        self.name = name
        self.values = []

    def add_definition(self, definition):
        '''
        Add a string flag definition.
        '''
        assert(definition.startswith(self.name))
        value = definition[len(self.name):]
        if value.startswith('='):
            self.values.append(('==', value[1:]))
        elif value.startswith('!='):
            self.values.append(('!=', value[2:]))
        else:
            return errors.fatal('Malformed flag: %s' % definition)

    def matches(self, value):
        '''
        Return whether one of the string flag definitions matches the given
        value.
        For example,
            flag = StringFlag('foo')
            flag.add_definition('foo!=bar')
            flag.matches('bar') returns False
            flag.matches('qux') returns True
            flag = StringFlag('foo')
            flag.add_definition('foo=bar')
            flag.add_definition('foo=baz')
            flag.matches('bar') returns True
            flag.matches('baz') returns True
            flag.matches('qux') returns False
        '''
        if not self.values:
            return True
        for comparison, val in self.values:
            if eval('value %s val' % comparison):
                return True
        return False

    def __str__(self):
        '''
        Serialize the flag definitions in the same form given to each
        add_definition() call.
        '''
        res = []
        for comparison, val in self.values:
            if comparison == '==':
                res.append('%s=%s' % (self.name, val))
            else:
                res.append('%s!=%s' % (self.name, val))
        return ' '.join(res)


class VersionFlag(object):
    '''
    Class for version flags in manifest entries in the form:
        "flag=version"
        "flag<=version"
        "flag<version"
        "flag>=version"
        "flag>version"
    '''
    def __init__(self, name):
        '''
        Initialize a VersionFlag with the given name.
        '''
        self.name = name
        self.values = []

    def add_definition(self, definition):
        '''
        Add a version flag definition.
        '''
        assert(definition.startswith(self.name))
        value = definition[len(self.name):]
        if value.startswith('='):
            self.values.append(('==', LooseVersion(value[1:])))
        elif len(value) > 1 and value[0] in ['<', '>']:
            if value[1] == '=':
                if len(value) < 3:
                    return errors.fatal('Malformed flag: %s' % definition)
                self.values.append((value[0:2], LooseVersion(value[2:])))
            else:
                self.values.append((value[0], LooseVersion(value[1:])))
        else:
            return errors.fatal('Malformed flag: %s' % definition)

    def matches(self, value):
        '''
        Return whether one of the version flag definitions matches the given
        value.
        For example,
            flag = VersionFlag('foo')
            flag.add_definition('foo>=1.0')
            flag.matches('1.0') returns True
            flag.matches('1.1') returns True
            flag.matches('0.9') returns False
            flag = VersionFlag('foo')
            flag.add_definition('foo>=1.0')
            flag.add_definition('foo<0.5')
            flag.matches('0.4') returns True
            flag.matches('1.0') returns True
            flag.matches('0.6') returns False
        '''
        value = LooseVersion(value)
        if not self.values:
            return True
        for comparison, val in self.values:
            if eval('value %s val' % comparison):
                return True
        return False

    def __str__(self):
        '''
        Serialize the flag definitions in the same form given to each
        add_definition() call.
        '''
        res = []
        for comparison, val in self.values:
            if comparison == '==':
                res.append('%s=%s' % (self.name, val))
            else:
                res.append('%s%s%s' % (self.name, comparison, val))
        return ' '.join(res)


class Flags(OrderedDict):
    '''
    Class to handle a set of flags definitions given on a single manifest
    entry.
    '''
    FLAGS = {
        'application': StringFlag,
        'appversion': VersionFlag,
        'platformversion': VersionFlag,
        'contentaccessible': Flag,
        'os': StringFlag,
        'osversion': VersionFlag,
        'abi': StringFlag,
        'platform': Flag,
        'xpcnativewrappers': Flag,
        'tablet': Flag,
        'process': StringFlag,
    }
    RE = re.compile(r'([!<>=]+)')

    def __init__(self, *flags):
        '''
        Initialize a set of flags given in string form.
           flags = Flags('contentaccessible=yes', 'appversion>=3.5')
        '''
        OrderedDict.__init__(self)
        for f in flags:
            name = self.RE.split(f)
            name = name[0]
            if not name in self.FLAGS:
                errors.fatal('Unknown flag: %s' % name)
                continue
            if not name in self:
                self[name] = self.FLAGS[name](name)
            self[name].add_definition(f)

    def __str__(self):
        '''
        Serialize the set of flags.
        '''
        return ' '.join(str(self[k]) for k in self)

    def match(self, **filter):
        '''
        Return whether the set of flags match the set of given filters.
            flags = Flags('contentaccessible=yes', 'appversion>=3.5',
                          'application=foo')
            flags.match(application='foo') returns True
            flags.match(application='foo', appversion='3.5') returns True
            flags.match(application='foo', appversion='3.0') returns False
        '''
        for name, value in filter.iteritems():
            if not name in self:
                continue
            if not self[name].matches(value):
                return False
        return True
