# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
user preferences
"""
from __future__ import absolute_import, print_function

import json
import mozfile
import os
import tokenize

from six.moves.configparser import SafeConfigParser as ConfigParser
from six import StringIO, string_types

__all__ = ('PreferencesReadError', 'Preferences')


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
        - anything enclosed in single quotes will be treated as a string
          with the ''s removed from both sides
        """

        if not isinstance(value, string_types):
            return value  # no op
        quote = "'"
        if value == 'true':
            return True
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

        section = None  # for .ini files
        basename = os.path.basename(path)
        if ':' in basename:
            # section of INI file
            path, section = path.rsplit(':', 1)

        if not os.path.exists(path) and not mozfile.is_url(path):
            raise PreferencesReadError("'%s' does not exist" % path)

        if section:
            try:
                return cls.read_ini(path, section)
            except PreferencesReadError:
                raise
            except Exception as e:
                raise PreferencesReadError(str(e))

        # try both JSON and .ini format
        try:
            return cls.read_json(path)
        except Exception as e:
            try:
                return cls.read_ini(path)
            except Exception as f:
                for exception in e, f:
                    if isinstance(exception, PreferencesReadError):
                        raise exception
                raise PreferencesReadError("Could not recognize format of %s" % path)

    @classmethod
    def read_ini(cls, path, section=None):
        """read preferences from an .ini file"""

        parser = ConfigParser()
        parser.optionxform = str
        parser.readfp(mozfile.load(path))

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

        prefs = json.loads(mozfile.load(path).read())

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
        types = (bool, string_types, int)
        if [i for i in values if not [isinstance(i, j) for j in types]]:
            raise PreferencesReadError("Only bool, string, and int values allowed")
        return prefs

    @classmethod
    def read_prefs(cls, path, pref_setter='user_pref', interpolation=None):
        """
        Read preferences from (e.g.) prefs.js

        :param path: The path to the preference file to read.
        :param pref_setter: The name of the function used to set preferences
                            in the preference file.
        :param interpolation: If provided, a dict that will be passed
                              to str.format to interpolate preference values.
        """

        marker = '##//'  # magical marker
        lines = [i.strip() for i in mozfile.load(path).readlines()]
        _lines = []
        for line in lines:
            if not line.startswith(pref_setter):
                continue
            if '//' in line:
                line = line.replace('//', marker)
            _lines.append(line)
        string = '\n'.join(_lines)

        # skip trailing comments
        processed_tokens = []
        f_obj = StringIO(string)
        for token in tokenize.generate_tokens(f_obj.readline):
            if token[0] == tokenize.COMMENT:
                continue
            processed_tokens.append(token[:2])  # [:2] gets around http://bugs.python.org/issue9974
        string = tokenize.untokenize(processed_tokens)

        retval = []

        def pref(a, b):
            if interpolation and isinstance(b, string_types):
                b = b.format(**interpolation)
            retval.append((a, b))
        lines = [i.strip().rstrip(';') for i in string.split('\n') if i.strip()]

        _globals = {'retval': retval, 'true': True, 'false': False}
        _globals[pref_setter] = pref
        for line in lines:
            try:
                eval(line, _globals, {})
            except SyntaxError:
                print(line)
                raise

        # de-magic the marker
        for index, (key, value) in enumerate(retval):
            if isinstance(value, string_types) and marker in value:
                retval[index] = (key, value.replace(marker, '//'))

        return retval

    @classmethod
    def write(cls, _file, prefs, pref_string='user_pref(%s, %s);'):
        """write preferences to a file"""

        if isinstance(_file, string_types):
            f = open(_file, 'a')
        else:
            f = _file

        if isinstance(prefs, dict):
            # order doesn't matter
            prefs = prefs.items()

        # serialize -> JSON
        _prefs = [(json.dumps(k), json.dumps(v))
                  for k, v in prefs]

        # write the preferences
        for _pref in _prefs:
            print(pref_string % _pref, file=f)

        # close the file if opened internally
        if isinstance(_file, string_types):
            f.close()
