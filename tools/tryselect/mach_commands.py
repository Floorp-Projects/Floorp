# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
from collections import defaultdict

from mach.decorators import (
    CommandProvider,
    Command,
)

import mozpack.path as mozpath
from mozbuild.base import BuildEnvironmentNotFoundException, MachCommandBase

CONFIG_ENVIRONMENT_NOT_FOUND = '''
No config environment detected. This means we are unable to properly
detect test files in the specified paths or tags. Please run:

    $ mach configure

and try again.
'''.lstrip()


def syntax_parser():
    from tryselect.selectors.syntax import arg_parser
    parser = arg_parser()
    # The --no-artifact flag is only interpreted locally by |mach try|; it's not
    # like the --artifact flag, which is interpreted remotely by the try server.
    #
    # We need a tri-state where set is different than the default value, so we
    # use a different variable than --artifact.
    parser.add_argument('--no-artifact',
                        dest='no_artifact',
                        action='store_true',
                        help='Force compiled (non-artifact) builds even when '
                             '--enable-artifact-builds is set.')
    return parser


@CommandProvider
class PushToTry(MachCommandBase):
    def normalise_list(self, items, allow_subitems=False):
        from tryselect.selectors.syntax import parse_arg

        rv = defaultdict(list)
        for item in items:
            parsed = parse_arg(item)
            for key, values in parsed.iteritems():
                rv[key].extend(values)

        if not allow_subitems:
            if not all(item == [] for item in rv.itervalues()):
                raise ValueError("Unexpected subitems in argument")
            return rv.keys()
        else:
            return rv

    def validate_args(self, **kwargs):
        from tryselect.selectors.syntax import AutoTry

        tests_selected = kwargs["tests"] or kwargs["paths"] or kwargs["tags"]
        if kwargs["platforms"] is None and (kwargs["jobs"] is None or tests_selected):
            if 'AUTOTRY_PLATFORM_HINT' in os.environ:
                kwargs["platforms"] = [os.environ['AUTOTRY_PLATFORM_HINT']]
            elif tests_selected:
                print("Must specify platform when selecting tests.")
                sys.exit(1)
            else:
                print("Either platforms or jobs must be specified as an argument to "
                      "|mach try syntax|.")
                sys.exit(1)

        try:
            platforms = (self.normalise_list(kwargs["platforms"])
                         if kwargs["platforms"] else {})
        except ValueError as e:
            print("Error parsing -p argument:\n%s" % e.message)
            sys.exit(1)

        try:
            tests = (self.normalise_list(kwargs["tests"], allow_subitems=True)
                     if kwargs["tests"] else {})
        except ValueError as e:
            print("Error parsing -u argument (%s):\n%s" % (kwargs["tests"], e.message))
            sys.exit(1)

        try:
            talos = (self.normalise_list(kwargs["talos"], allow_subitems=True)
                     if kwargs["talos"] else [])
        except ValueError as e:
            print("Error parsing -t argument:\n%s" % e.message)
            sys.exit(1)

        try:
            jobs = (self.normalise_list(kwargs["jobs"]) if kwargs["jobs"] else {})
        except ValueError as e:
            print("Error parsing -j argument:\n%s" % e.message)
            sys.exit(1)

        paths = []
        for p in kwargs["paths"]:
            p = mozpath.normpath(os.path.abspath(p))
            if not (os.path.isdir(p) and p.startswith(self.topsrcdir)):
                print('Specified path "%s" is not a directory under the srcdir,'
                      ' unable to specify tests outside of the srcdir' % p)
                sys.exit(1)
            if len(p) <= len(self.topsrcdir):
                print('Specified path "%s" is at the top of the srcdir and would'
                      ' select all tests.' % p)
                sys.exit(1)
            paths.append(os.path.relpath(p, self.topsrcdir))

        try:
            tags = self.normalise_list(kwargs["tags"]) if kwargs["tags"] else []
        except ValueError as e:
            print("Error parsing --tags argument:\n%s" % e.message)
            sys.exit(1)

        extra_values = {k['dest'] for k in AutoTry.pass_through_arguments.values()}
        extra_args = {k: v for k, v in kwargs.items()
                      if k in extra_values and v}

        return kwargs["builds"], platforms, tests, talos, jobs, paths, tags, extra_args

    @Command('try',
             category='testing',
             description='Push selected tests to the try server',
             parser=syntax_parser)
    def syntax(self, **kwargs):
        """Push the current tree to try, with the specified syntax.

        Build options, platforms and regression tests may be selected
        using the usual try options (-b, -p and -u respectively). In
        addition, tests in a given directory may be automatically
        selected by passing that directory as a positional argument to the
        command. For example:

        mach try -b d -p linux64 dom testing/web-platform/tests/dom

        would schedule a try run for linux64 debug consisting of all
        tests under dom/ and testing/web-platform/tests/dom.

        Test selection using positional arguments is available for
        mochitests, reftests, xpcshell tests and web-platform-tests.

        Tests may be also filtered by passing --tag to the command,
        which will run only tests marked as having the specified
        tags e.g.

        mach try -b d -p win64 --tag media

        would run all tests tagged 'media' on Windows 64.

        If both positional arguments or tags and -u are supplied, the
        suites in -u will be run in full. Where tests are selected by
        positional argument they will be run in a single chunk.

        If no build option is selected, both debug and opt will be
        scheduled. If no platform is selected a default is taken from
        the AUTOTRY_PLATFORM_HINT environment variable, if set.

        The command requires either its own mercurial extension ("push-to-try",
        installable from mach mercurial-setup) or a git repo using git-cinnabar
        (available at https://github.com/glandium/git-cinnabar).

        """

        from mozbuild.testing import TestResolver
        from tryselect.selectors.syntax import AutoTry

        print("mach try is under development, please file bugs blocking 1149670.")

        def resolver_func():
            return self._spawn(TestResolver)

        at = AutoTry(self.topsrcdir, resolver_func, self._mach_context)

        if kwargs["list"]:
            at.list_presets()
            sys.exit()

        if kwargs["load"] is not None:
            defaults = at.load_config(kwargs["load"])

            if defaults is None:
                print("No saved configuration called %s found in autotry.ini" % kwargs["load"],
                      file=sys.stderr)

            for key, value in kwargs.iteritems():
                if value in (None, []) and key in defaults:
                    kwargs[key] = defaults[key]

        if kwargs["push"] and at.find_uncommited_changes():
            print('ERROR please commit changes before continuing')
            sys.exit(1)

        if not any(kwargs[item] for item in ("paths", "tests", "tags")):
            kwargs["paths"], kwargs["tags"] = at.find_paths_and_tags(kwargs["verbose"])

        builds, platforms, tests, talos, jobs, paths, tags, extra = self.validate_args(**kwargs)

        if paths or tags:
            if not os.path.exists(os.path.join(self.topobjdir, 'config.status')):
                print(CONFIG_ENVIRONMENT_NOT_FOUND)
                sys.exit(1)

            paths = [os.path.relpath(os.path.normpath(os.path.abspath(item)), self.topsrcdir)
                     for item in paths]
            paths_by_flavor = at.paths_by_flavor(paths=paths, tags=tags)

            if not paths_by_flavor and not tests:
                print("No tests were found when attempting to resolve paths:\n\n\t%s" %
                      paths)
                sys.exit(1)

            if not kwargs["intersection"]:
                paths_by_flavor = at.remove_duplicates(paths_by_flavor, tests)
        else:
            paths_by_flavor = {}

        # No point in dealing with artifacts if we aren't running any builds
        local_artifact_build = False
        if platforms:
            try:
                if self.substs.get("MOZ_ARTIFACT_BUILDS"):
                    local_artifact_build = True
            except BuildEnvironmentNotFoundException:
                # If we don't have a build locally, we can't tell whether
                # an artifact build is desired, but we still want the
                # command to succeed, if possible.
                pass

            # Add --artifact if --enable-artifact-builds is set ...
            if local_artifact_build:
                extra["artifact"] = True
            # ... unless --no-artifact is explicitly given.
            if kwargs["no_artifact"]:
                if "artifact" in extra:
                    del extra["artifact"]

        try:
            msg = at.calc_try_syntax(platforms, tests, talos, jobs, builds, paths_by_flavor, tags,
                                     extra, kwargs["intersection"])
        except ValueError as e:
            print(e.message)
            sys.exit(1)

        if local_artifact_build:
            if kwargs["no_artifact"]:
                print('mozconfig has --enable-artifact-builds but '
                      '--no-artifact specified, not including --artifact '
                      'flag in try syntax')
            else:
                print('mozconfig has --enable-artifact-builds; including '
                      '--artifact flag in try syntax (use --no-artifact '
                      'to override)')

        if kwargs["verbose"] and paths_by_flavor:
            print('The following tests will be selected: ')
            for flavor, paths in paths_by_flavor.iteritems():
                print("%s: %s" % (flavor, ",".join(paths)))

        if kwargs["verbose"] or not kwargs["push"]:
            print('The following try syntax was calculated:\n%s' % msg)

        if kwargs["push"]:
            at.push_to_try(msg, kwargs["verbose"])

        if kwargs["save"] is not None:
            at.save_config(kwargs["save"], msg)
