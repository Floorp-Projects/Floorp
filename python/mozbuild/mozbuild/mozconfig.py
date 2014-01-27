# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import re
import subprocess

from collections import defaultdict
from mach.mixin.process import ProcessExecutionMixin


MOZ_MYCONFIG_ERROR = '''
The MOZ_MYCONFIG environment variable to define the location of mozconfigs
is deprecated. If you wish to define the mozconfig path via an environment
variable, use MOZCONFIG instead.
'''.strip()

MOZCONFIG_LEGACY_PATH = '''
You currently have a mozconfig at %s. This implicit location is no longer
supported. Please move it to %s/.mozconfig or set an explicit path
via the $MOZCONFIG environment variable.
'''.strip()

MOZCONFIG_BAD_EXIT_CODE = '''
Evaluation of your mozconfig exited with an error. This could be triggered
by a command inside your mozconfig failing. Please change your mozconfig
to not error and/or to catch errors in executed commands.
'''.strip()


class MozconfigFindException(Exception):
    """Raised when a mozconfig location is not defined properly."""


class MozconfigLoadException(Exception):
    """Raised when a mozconfig could not be loaded properly.

    This typically indicates a malformed or misbehaving mozconfig file.
    """

    def __init__(self, path, message, output=None):
        self.path = path
        self.output = output
        Exception.__init__(self, message)


class MozconfigLoader(ProcessExecutionMixin):
    """Handles loading and parsing of mozconfig files."""

    RE_MAKE_VARIABLE = re.compile('''
        ^\s*                    # Leading whitespace
        (?P<var>[a-zA-Z_0-9]+)  # Variable name
        \s* [?:]?= \s*          # Assignment operator surrounded by optional
                                # spaces
        (?P<value>.*$)''',      # Everything else (likely the value)
        re.VERBOSE)

    # Default mozconfig files in the topsrcdir.
    DEFAULT_TOPSRCDIR_PATHS = ('.mozconfig', 'mozconfig')

    DEPRECATED_TOPSRCDIR_PATHS = ('mozconfig.sh', 'myconfig.sh')
    DEPRECATED_HOME_PATHS = ('.mozconfig', '.mozconfig.sh', '.mozmyconfig.sh')

    IGNORE_SHELL_VARIABLES = ('_')

    def __init__(self, topsrcdir):
        self.topsrcdir = topsrcdir

    @property
    def _loader_script(self):
        our_dir = os.path.abspath(os.path.dirname(__file__))

        return os.path.join(our_dir, 'mozconfig_loader')

    def find_mozconfig(self):
        """Find the active mozconfig file for the current environment.

        This emulates the logic in mozconfig-find.

        1) If ENV[MOZCONFIG] is set, use that
        2) If $TOPSRCDIR/mozconfig or $TOPSRCDIR/.mozconfig exists, use it.
        3) If both exist or if there are legacy locations detected, error out.

        The absolute path to the found mozconfig will be returned on success.
        None will be returned if no mozconfig could be found. A
        MozconfigFindException will be raised if there is a bad state,
        including conditions from #3 above.
        """
        # Check for legacy methods first.

        if 'MOZ_MYCONFIG' in os.environ:
            raise MozconfigFindException(MOZ_MYCONFIG_ERROR)

        env_path = os.environ.get('MOZCONFIG', None)
        if env_path is not None:
            if not os.path.exists(env_path):
                raise MozconfigFindException(
                    'MOZCONFIG environment variable refers to a path that '
                    'does not exist: ' + env_path)

            if not os.path.isfile(env_path):
                raise MozconfigFindException(
                    'MOZCONFIG environment variable refers to a '
                    'non-file: ' + env_path)

        srcdir_paths = [os.path.join(self.topsrcdir, p) for p in
            self.DEFAULT_TOPSRCDIR_PATHS]
        existing = [p for p in srcdir_paths if os.path.isfile(p)]

        if env_path is None and len(existing) > 1:
            raise MozconfigFindException('Multiple default mozconfig files '
                'present. Remove all but one. ' + ', '.join(existing))

        path = None

        if env_path is not None:
            path = env_path
        elif len(existing):
            assert len(existing) == 1
            path = existing[0]

        if path is not None:
            return os.path.abspath(path)

        deprecated_paths = [os.path.join(self.topsrcdir, s) for s in
            self.DEPRECATED_TOPSRCDIR_PATHS]

        home = os.environ.get('HOME', None)
        if home is not None:
            deprecated_paths.extend([os.path.join(home, s) for s in
            self.DEPRECATED_HOME_PATHS])

        for path in deprecated_paths:
            if os.path.exists(path):
                raise MozconfigFindException(
                    MOZCONFIG_LEGACY_PATH % (path, self.topsrcdir))

        return None

    def read_mozconfig(self, path=None, moz_build_app=None):
        """Read the contents of a mozconfig into a data structure.

        This takes the path to a mozconfig to load. If it is not defined, we
        will try to find a mozconfig from the environment using
        find_mozconfig().

        mozconfig files are shell scripts. So, we can't just parse them.
        Instead, we run the shell script in a wrapper which allows us to record
        state from execution. Thus, the output from a mozconfig is a friendly
        static data structure.
        """
        if path is None:
            path = self.find_mozconfig()

        result = {
            'path': path,
            'topobjdir': None,
            'configure_args': None,
            'make_flags': None,
            'make_extra': None,
            'env': None,
        }

        if path is None:
            return result

        path = path.replace(os.sep, '/')

        result['configure_args'] = []
        result['make_extra'] = []
        result['make_flags'] = []

        env = dict(os.environ)

        args = self._normalize_command([self._loader_script,
            self.topsrcdir.replace(os.sep, '/'), path], True)

        try:
            # We need to capture stderr because that's where the shell sends
            # errors if execution fails.
            output = subprocess.check_output(args, stderr=subprocess.STDOUT,
                cwd=self.topsrcdir, env=env)
        except subprocess.CalledProcessError as e:
            lines = e.output.splitlines()

            # Output before actual execution shouldn't be relevant.
            try:
                index = lines.index('------END_BEFORE_SOURCE')
                lines = lines[index + 1:]
            except ValueError:
                pass

            raise MozconfigLoadException(path, MOZCONFIG_BAD_EXIT_CODE, lines)

        parsed = self._parse_loader_output(output)

        all_variables = set(parsed['vars_before'].keys())
        all_variables |= set(parsed['vars_after'].keys())

        changed = {
            'added': {},
            'removed': {},
            'modified': {},
            'unmodified': {},
        }

        for key in all_variables:
            if key in self.IGNORE_SHELL_VARIABLES:
                continue

            if key not in parsed['vars_before']:
                changed['added'][key] = parsed['vars_after'][key]
                continue

            if key not in parsed['vars_after']:
                changed['removed'][key] = parsed['vars_before'][key]
                continue

            if parsed['vars_before'][key] != parsed['vars_after'][key]:
                changed['modified'][key] = (
                    parsed['vars_before'][key], parsed['vars_after'][key])
                continue

            changed['unmodified'][key] = parsed['vars_after'][key]

        result['env'] = changed

        result['configure_args'] = [self._expand(o) for o in parsed['ac']]

        if moz_build_app is not None:
            result['configure_args'].extend(self._expand(o) for o in
                parsed['ac_app'][moz_build_app])

        mk = [self._expand(o) for o in parsed['mk']]

        for o in mk:
            match = self.RE_MAKE_VARIABLE.match(o)

            if match is None:
                result['make_extra'].append(o)
                continue

            name, value = match.group('var'), match.group('value')

            if name == 'MOZ_MAKE_FLAGS':
                result['make_flags'] = value
                continue

            if name == 'MOZ_OBJDIR':
                result['topobjdir'] = value
                continue

            result['make_extra'].append(o)

        return result

    def _parse_loader_output(self, output):
        mk_options = []
        ac_options = []
        ac_app_options = defaultdict(list)
        before_source = {}
        after_source = {}

        current = None
        current_type = None
        in_variable = None

        for line in output.splitlines():

            # XXX This is an ugly hack. Data may be lost from things
            # like environment variable values.
            # See https://bugzilla.mozilla.org/show_bug.cgi?id=831381
            line = line.decode('utf-8', 'ignore')

            if not line:
                continue

            if line.startswith('------BEGIN_'):
                assert current_type is None
                assert current is None
                assert not in_variable
                current_type = line[len('------BEGIN_'):]
                current = []
                continue

            if line.startswith('------END_'):
                assert not in_variable
                section = line[len('------END_'):]
                assert current_type == section

                if current_type == 'AC_OPTION':
                    ac_options.append('\n'.join(current))
                elif current_type == 'MK_OPTION':
                    mk_options.append('\n'.join(current))
                elif current_type == 'AC_APP_OPTION':
                    app = current.pop(0)
                    ac_app_options[app].append('\n'.join(current))

                current = None
                current_type = None
                continue

            assert current_type is not None

            if current_type in ('BEFORE_SOURCE', 'AFTER_SOURCE'):
                # mozconfigs are sourced using the Bourne shell (or at least
                # in Bourne shell mode). This means |set| simply lists
                # variables from the current shell (not functions). (Note that
                # if Bash is installed in /bin/sh it acts like regular Bourne
                # and doesn't print functions.) So, lines should have the
                # form:
                #
                #  key='value'
                #  key=value
                #
                # The only complication is multi-line variables. Those have the
                # form:
                #
                #  key='first
                #  second'

                # TODO Bug 818377 Properly handle multi-line variables of form:
                # $ foo="a='b'
                # c='d'"
                # $ set
                # foo='a='"'"'b'"'"'
                # c='"'"'d'"'"

                name = in_variable
                value = None
                if in_variable:
                    # Reached the end of a multi-line variable.
                    if line.endswith("'") and not line.endswith("\\'"):
                        current.append(line[:-1])
                        value = '\n'.join(current)
                        in_variable = None
                    else:
                        current.append(line)
                        continue
                else:
                    equal_pos = line.find('=')

                    if equal_pos < 1:
                        # TODO log warning?
                        continue

                    name = line[0:equal_pos]
                    value = line[equal_pos + 1:]

                    if len(value):
                        has_quote = value[0] == "'"

                        if has_quote:
                            value = value[1:]

                        # Lines with a quote not ending in a quote are multi-line.
                        if has_quote and not value.endswith("'"):
                            in_variable = name
                            current.append(value)
                            continue
                        else:
                            value = value[:-1] if has_quote else value

                assert name is not None

                if current_type == 'BEFORE_SOURCE':
                    before_source[name] = value
                else:
                    after_source[name] = value

                current = []

                continue

            current.append(line)

        return {
            'mk': mk_options,
            'ac': ac_options,
            'ac_app': ac_app_options,
            'vars_before': before_source,
            'vars_after': after_source,
        }

    def _expand(self, s):
        return s.replace('@TOPSRCDIR@', self.topsrcdir)
