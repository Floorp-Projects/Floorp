# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import filecmp
import os
import re
import sys
import subprocess
import traceback

from mozpack import path as mozpath
from mozbuild.util import system_encoding


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

MOZCONFIG_BAD_OUTPUT = '''
Evaluation of your mozconfig produced unexpected output.  This could be
triggered by a command inside your mozconfig failing or producing some warnings
or error messages. Please change your mozconfig to not error and/or to catch
errors in executed commands.
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


class MozconfigLoader(object):
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

    IGNORE_SHELL_VARIABLES = {'_', 'BASH_ARGV', 'BASH_ARGV0', 'BASH_ARGC'}

    ENVIRONMENT_VARIABLES = {
        'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS', 'MOZ_OBJDIR',
    }

    AUTODETECT = object()

    def __init__(self, topsrcdir):
        self.topsrcdir = topsrcdir

    @property
    def _loader_script(self):
        our_dir = os.path.abspath(os.path.dirname(__file__))

        return os.path.join(our_dir, 'mozconfig_loader')

    def find_mozconfig(self, env=os.environ):
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

        if 'MOZ_MYCONFIG' in env:
            raise MozconfigFindException(MOZ_MYCONFIG_ERROR)

        env_path = env.get('MOZCONFIG', None) or None
        if env_path is not None:
            if not os.path.isabs(env_path):
                potential_roots = [self.topsrcdir, os.getcwd()]
                # Attempt to eliminate duplicates for e.g.
                # self.topsrcdir == os.curdir.
                potential_roots = set(os.path.abspath(p) for p in potential_roots)
                existing = [root for root in potential_roots
                            if os.path.exists(os.path.join(root, env_path))]
                if len(existing) > 1:
                    # There are multiple files, but we might have a setup like:
                    #
                    # somedirectory/
                    #   srcdir/
                    #   objdir/
                    #
                    # MOZCONFIG=../srcdir/some/path/to/mozconfig
                    #
                    # and be configuring from the objdir.  So even though we
                    # have multiple existing files, they are actually the same
                    # file.
                    mozconfigs = [os.path.join(root, env_path)
                                  for root in existing]
                    if not all(map(lambda p1, p2: filecmp.cmp(p1, p2, shallow=False),
                                   mozconfigs[:-1], mozconfigs[1:])):
                        raise MozconfigFindException(
                            'MOZCONFIG environment variable refers to a path that ' +
                            'exists in more than one of ' + ', '.join(potential_roots) +
                            '. Remove all but one.')
                elif not existing:
                    raise MozconfigFindException(
                        'MOZCONFIG environment variable refers to a path that ' +
                        'does not exist in any of ' + ', '.join(potential_roots))

                env_path = os.path.join(existing[0], env_path)
            elif not os.path.exists(env_path):  # non-relative path
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

        home = env.get('HOME', None)
        if home is not None:
            deprecated_paths.extend([os.path.join(home, s) for s in
                                     self.DEPRECATED_HOME_PATHS])

        for path in deprecated_paths:
            if os.path.exists(path):
                raise MozconfigFindException(
                    MOZCONFIG_LEGACY_PATH % (path, self.topsrcdir))

        return None

    def read_mozconfig(self, path=None):
        """Read the contents of a mozconfig into a data structure.

        This takes the path to a mozconfig to load. If the given path is
        AUTODETECT, will try to find a mozconfig from the environment using
        find_mozconfig().

        mozconfig files are shell scripts. So, we can't just parse them.
        Instead, we run the shell script in a wrapper which allows us to record
        state from execution. Thus, the output from a mozconfig is a friendly
        static data structure.
        """
        if path is self.AUTODETECT:
            path = self.find_mozconfig()

        result = {
            'path': path,
            'topobjdir': None,
            'configure_args': None,
            'make_flags': None,
            'make_extra': None,
            'env': None,
            'vars': None,
        }

        if path is None:
            return result

        path = mozpath.normsep(path)

        result['configure_args'] = []
        result['make_extra'] = []
        result['make_flags'] = []

        env = dict(os.environ)

        # Since mozconfig_loader is a shell script, running it "normally"
        # actually leads to two shell executions on Windows. Avoid this by
        # directly calling sh mozconfig_loader.
        shell = 'sh'
        if 'MOZILLABUILD' in os.environ:
            shell = os.environ['MOZILLABUILD'] + '/msys/bin/sh'
        if sys.platform == 'win32':
            shell = shell + '.exe'

        command = [shell, mozpath.normsep(self._loader_script),
                   mozpath.normsep(self.topsrcdir), path, sys.executable,
                   mozpath.join(mozpath.dirname(self._loader_script),
                                'action', 'dump_env.py')]

        try:
            # We need to capture stderr because that's where the shell sends
            # errors if execution fails.
            output = subprocess.check_output(command, stderr=subprocess.STDOUT,
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

        try:
            parsed = self._parse_loader_output(output)
        except AssertionError:
            # _parse_loader_output uses assertions to verify the
            # well-formedness of the shell output; when these fail, it
            # generally means there was a problem with the output, but we
            # include the assertion traceback just to be sure.
            print('Assertion failed in _parse_loader_output:')
            traceback.print_exc()
            raise MozconfigLoadException(path, MOZCONFIG_BAD_OUTPUT,
                                         output.splitlines())

        def diff_vars(vars_before, vars_after):
            set1 = set(vars_before.keys()) - self.IGNORE_SHELL_VARIABLES
            set2 = set(vars_after.keys()) - self.IGNORE_SHELL_VARIABLES
            added = set2 - set1
            removed = set1 - set2
            maybe_modified = set1 & set2
            changed = {
                'added': {},
                'removed': {},
                'modified': {},
                'unmodified': {},
            }

            for key in added:
                changed['added'][key] = vars_after[key]

            for key in removed:
                changed['removed'][key] = vars_before[key]

            for key in maybe_modified:
                if vars_before[key] != vars_after[key]:
                    changed['modified'][key] = (
                        vars_before[key], vars_after[key])
                elif key in self.ENVIRONMENT_VARIABLES:
                    # In order for irrelevant environment variable changes not
                    # to incur in re-running configure, only a set of
                    # environment variables are stored when they are
                    # unmodified. Otherwise, changes such as using a different
                    # terminal window, or even rebooting, would trigger
                    # reconfigures.
                    changed['unmodified'][key] = vars_after[key]

            return changed

        result['env'] = diff_vars(parsed['env_before'], parsed['env_after'])

        # Environment variables also appear as shell variables, but that's
        # uninteresting duplication of information. Filter them out.
        def filt(x, y): return {k: v for k, v in x.items() if k not in y}
        result['vars'] = diff_vars(
            filt(parsed['vars_before'], parsed['env_before']),
            filt(parsed['vars_after'], parsed['env_after'])
        )

        result['configure_args'] = [self._expand(o) for o in parsed['ac']]

        if 'MOZ_OBJDIR' in parsed['env_before']:
            result['topobjdir'] = parsed['env_before']['MOZ_OBJDIR']

        mk = [self._expand(o) for o in parsed['mk']]

        for o in mk:
            match = self.RE_MAKE_VARIABLE.match(o)

            if match is None:
                result['make_extra'].append(o)
                continue

            name, value = match.group('var'), match.group('value')

            if name == 'MOZ_MAKE_FLAGS':
                result['make_flags'] = value.split()
                continue

            if name == 'MOZ_OBJDIR':
                result['topobjdir'] = value
                continue

            result['make_extra'].append(o)

        return result

    def _parse_loader_output(self, output):
        mk_options = []
        ac_options = []
        before_source = {}
        after_source = {}
        env_before_source = {}
        env_after_source = {}

        current = None
        current_type = None
        in_variable = None

        for line in output.splitlines():

            # XXX This is an ugly hack. Data may be lost from things
            # like environment variable values.
            # See https://bugzilla.mozilla.org/show_bug.cgi?id=831381
            line = line.decode(system_encoding, 'ignore')

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

                current = None
                current_type = None
                continue

            assert current_type is not None

            vars_mapping = {
                'BEFORE_SOURCE': before_source,
                'AFTER_SOURCE': after_source,
                'ENV_BEFORE_SOURCE': env_before_source,
                'ENV_AFTER_SOURCE': env_after_source,
            }

            if current_type in vars_mapping:
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

                vars_mapping[current_type][name] = value

                current = []

                continue

            current.append(line)

        return {
            'mk': mk_options,
            'ac': ac_options,
            'vars_before': before_source,
            'vars_after': after_source,
            'env_before': env_before_source,
            'env_after': env_after_source,
        }

    def _expand(self, s):
        return s.replace('@TOPSRCDIR@', self.topsrcdir)
