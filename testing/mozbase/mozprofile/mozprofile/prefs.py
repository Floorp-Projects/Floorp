#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozprofile.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Clint Talbert <ctalbert@mozilla.com>
#   Jeff Hammel <jhammel@mozilla.com>
#   Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""
user preferences
"""

import os
import re
from ConfigParser import SafeConfigParser as ConfigParser

try:
    import json
except ImportError:
    import simplejson as json

class PreferencesReadError(Exception):
    """read error for prefrences files"""


class Preferences(object):
    """assembly of preferences from various sources"""

    def __init__(self, prefs=None):
        self._prefs = []
        if prefs:
            self.add(prefs)

    def add(self, prefs, cast=False):
        """
        - cast: whether to cast strings to value, e.g. '1' -> 1
        """
        # wants a list of 2-tuples
        if isinstance(prefs, dict):
            prefs = prefs.items()
        if cast:
            prefs = [(i, self.cast(j)) for i, j in prefs]
        self._prefs += prefs

    def add_file(self, path):
        """a preferences from a file"""
        self.add(self.read(path))

    def __call__(self):
        return self._prefs

    @classmethod
    def cast(cls, value):
        """
        interpolate a preference from a string
        from the command line or from e.g. an .ini file, there is no good way to denote
        what type the preference value is, as natively it is a string
        - integers will get cast to integers
        - true/false will get cast to True/False
        - anything enclosed in single quotes will be treated as a string with the ''s removed from both sides
        """

        if not isinstance(value, basestring):
            return value # no op
        quote = "'"
        if value == 'true':
            return  True
        if value == 'false':
            return False
        try:
            return int(value)
        except ValueError:
            pass
        if value.startswith(quote) and value.endswith(quote):
            value = value[1:-1]
        return value


    @classmethod
    def read(cls, path):
        """read preferences from a file"""

        section = None # for .ini files
        basename = os.path.basename(path)
        if ':' in basename:
            # section of INI file
            path, section = path.rsplit(':', 1)

        if not os.path.exists(path):
            raise PreferencesReadError("'%s' does not exist" % path)

        if section:
            try:
                return cls.read_ini(path, section)
            except PreferencesReadError:
                raise
            except Exception, e:
                raise PreferencesReadError(str(e))

        # try both JSON and .ini format
        try:
            return cls.read_json(path)
        except Exception, e:
            try:
                return cls.read_ini(path)
            except Exception, f:
                for exception in e, f:
                    if isinstance(exception, PreferencesReadError):
                        raise exception
                raise PreferencesReadError("Could not recognize format of %s" % path)


    @classmethod
    def read_ini(cls, path, section=None):
        """read preferences from an .ini file"""

        parser = ConfigParser()
        parser.read(path)

        if section:
            if section not in parser.sections():
                raise PreferencesReadError("No section '%s' in %s" % (section, path))
            retval = parser.items(section, raw=True)
        else:
            retval = parser.defaults().items()

        # cast the preferences since .ini is just strings
        return [(i, cls.cast(j)) for i, j in retval]

    @classmethod
    def read_json(cls, path):
        """read preferences from a JSON blob"""

        prefs = json.loads(file(path).read())

        if type(prefs) not in [list, dict]:
            raise PreferencesReadError("Malformed preferences: %s" % path)
        if isinstance(prefs, list):
            if [i for i in prefs if type(i) != list or len(i) != 2]:
                raise PreferencesReadError("Malformed preferences: %s" % path)
            values = [i[1] for i in prefs]
        elif isinstance(prefs, dict):
            values = prefs.values()
        else:
            raise PreferencesReadError("Malformed preferences: %s" % path)
        types = (bool, basestring, int)
        if [i for i in values
            if not [isinstance(i, j) for j in types]]:
            raise PreferencesReadError("Only bool, string, and int values allowed")
        return prefs

    @classmethod
    def read_prefs(cls, path, pref_setter='user_pref'):
        """read preferences from (e.g.) prefs.js"""

        comment = re.compile('/\*([^*]|[\r\n]|(\*+([^*/]|[\r\n])))*\*+/', re.MULTILINE)

        token = '##//' # magical token
        lines = [i.strip() for i in file(path).readlines() if i.strip()]
        _lines = []
        for line in lines:
            if line.startswith('#'):
                continue
            if '//' in line:
                line = line.replace('//', token)
            _lines.append(line)
        string = '\n'.join(_lines)
        string = re.sub(comment, '', string)

        retval = []
        def pref(a, b):
            retval.append((a, b))
        lines = [i.strip().rstrip(';') for i in string.split('\n') if i.strip()]

        _globals = {'retval': retval, 'true': True, 'false': False}
        _globals[pref_setter] = pref
        for line in lines:
            try:
                eval(line, _globals, {})
            except SyntaxError:
                print line
                raise

        # de-magic the token
        for index, (key, value) in enumerate(retval):
            if isinstance(value, basestring) and token in value:
                retval[index] = (key, value.replace(token, '//'))

        return retval

    @classmethod
    def write(_file, prefs, pref_string='user_pref("%s", %s);'):
        """write preferences to a file"""

        if isinstance(_file, basestring):
            f = file(_file, 'w')
        else:
            f = _file

        if isinstance(prefs, dict):
            prefs = prefs.items()

        for key, value in prefs:
            if value is True:
                print >> f, pref_string % (key, 'true')
            elif value is False:
                print >> f, pref_string % (key, 'false')
            elif isinstance(value, basestring):
                print >> f, pref_string % (key, repr(string(value)))
            else:
                print >> f, pref_string % (key, value) # should be numeric!

        if isinstance(_file, basestring):
            f.close()

if __name__ == '__main__':
    pass
