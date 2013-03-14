# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
user preferences
"""

__all__ = ('PreferencesReadError', 'Preferences')

import os
import re
import tokenize
from ConfigParser import SafeConfigParser as ConfigParser
from StringIO import StringIO

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
        :param prefs:
        :param cast: whether to cast strings to value, e.g. '1' -> 1
        """
        # wants a list of 2-tuples
        if isinstance(prefs, dict):
            prefs = prefs.items()
        if cast:
            prefs = [(i, self.cast(j)) for i, j in prefs]
        self._prefs += prefs

    def add_file(self, path):
        """a preferences from a file
        
        :param path:
        """
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

        marker = '##//' # magical marker
        lines = [i.strip() for i in file(path).readlines() if i.strip()]
        _lines = []
        for line in lines:
            if line.startswith(('#', '//')):
                continue
            if '//' in line:
                line = line.replace('//', marker)
            _lines.append(line)
        string = '\n'.join(_lines)
        string = re.sub(comment, '', string)

        # skip trailing comments
        processed_tokens = []
        f_obj = StringIO(string)
        for token in tokenize.generate_tokens(f_obj.readline):
            if token[0] == tokenize.COMMENT:
                continue
            processed_tokens.append(token[:2]) # [:2] gets around http://bugs.python.org/issue9974
        string = tokenize.untokenize(processed_tokens)

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

        # de-magic the marker
        for index, (key, value) in enumerate(retval):
            if isinstance(value, basestring) and marker in value:
                retval[index] = (key, value.replace(marker, '//'))

        return retval

    @classmethod
    def write(cls, _file, prefs, pref_string='user_pref("%s", %s);'):
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
                print >> f, pref_string % (key, repr(str(value)))
            else:
                print >> f, pref_string % (key, value) # should be numeric!

        if isinstance(_file, basestring):
            f.close()
