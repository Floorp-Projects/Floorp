# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase

from mozbuild.backend import (
    backends,
)

BUILD_WHAT_HELP = '''
What to build. Can be a top-level make target or a relative directory. If
multiple options are provided, they will be built serially. Takes dependency
information from `topsrcdir/build/dumbmake-dependencies` to build additional
targets as needed. BUILDING ONLY PARTS OF THE TREE CAN RESULT IN BAD TREE
STATE. USE AT YOUR OWN RISK.
'''.strip()


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command('build', category='build', description='Build the tree.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
                     help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('-C', '--directory', default=None,
                     help='Change to a subdirectory of the build directory first.')
    @CommandArgument('what', default=None, nargs='*', help=BUILD_WHAT_HELP)
    @CommandArgument('-X', '--disable-extra-make-dependencies',
                     default=False, action='store_true',
                     help='Do not add extra make dependencies.')
    @CommandArgument('-v', '--verbose', action='store_true',
                     help='Verbose output for what commands the build is running.')
    @CommandArgument('--keep-going', action='store_true',
                     help='Keep building after an error has occurred')
    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0,
              directory=None, verbose=False, keep_going=False):
        """Build the source tree.

        With no arguments, this will perform a full build.

        Positional arguments define targets to build. These can be make targets
        or patterns like "<dir>/<target>" to indicate a make target within a
        directory.

        There are a few special targets that can be used to perform a partial
        build faster than what `mach build` would perform:

        * binaries - compiles and links all C/C++ sources and produces shared
          libraries and executables (binaries).

        * faster - builds JavaScript, XUL, CSS, etc files.

        "binaries" and "faster" almost fully complement each other. However,
        there are build actions not captured by either. If things don't appear to
        be rebuilding, perform a vanilla `mach build` to rebuild the world.
        """
        from mozbuild.controller.building import (
            BuildDriver,
        )

        self.log_manager.enable_all_structured_loggers()

        driver = self._spawn(BuildDriver)
        return driver.build(
            what=what,
            disable_extra_make_dependencies=disable_extra_make_dependencies,
            jobs=jobs,
            directory=directory,
            verbose=verbose,
            keep_going=keep_going,
            mach_context=self._mach_context)

    @Command('configure', category='build',
             description='Configure the tree (run configure and config.status).')
    @CommandArgument('options', default=None, nargs=argparse.REMAINDER,
                     help='Configure options')
    def configure(self, options=None, buildstatus_messages=False, line_handler=None):
        from mozbuild.controller.building import (
            BuildDriver,
        )

        self.log_manager.enable_all_structured_loggers()
        driver = self._spawn(BuildDriver)

        return driver.configure(
            options=options,
            buildstatus_messages=buildstatus_messages,
            line_handler=line_handler)

    @Command('resource-usage', category='post-build',
             description='Show information about system resource usage for a build.')
    @CommandArgument('--address', default='localhost',
                     help='Address the HTTP server should listen on.')
    @CommandArgument('--port', type=int, default=0,
                     help='Port number the HTTP server should listen on.')
    @CommandArgument('--browser', default='firefox',
                     help='Web browser to automatically open. See webbrowser Python module.')
    @CommandArgument('--url',
                     help='URL of JSON document to display')
    def resource_usage(self, address=None, port=None, browser=None, url=None):
        import webbrowser
        from mozbuild.html_build_viewer import BuildViewerServer

        server = BuildViewerServer(address, port)

        if url:
            server.add_resource_json_url('url', url)
        else:
            last = self._get_state_filename('build_resources.json')
            if not os.path.exists(last):
                print('Build resources not available. If you have performed a '
                      'build and receive this message, the psutil Python package '
                      'likely failed to initialize properly.')
                return 1

            server.add_resource_json_file('last', last)
        try:
            webbrowser.get(browser).open_new_tab(server.url)
        except Exception:
            print('Cannot get browser specified, trying the default instead.')
            try:
                browser = webbrowser.get().open_new_tab(server.url)
            except Exception:
                print('Please open %s in a browser.' % server.url)

        print('Hit CTRL+c to stop server.')
        server.run()

    @Command('build-backend', category='build',
             description='Generate a backend used to build the tree.')
    @CommandArgument('-d', '--diff', action='store_true',
                     help='Show a diff of changes.')
    # It would be nice to filter the choices below based on
    # conditions, but that is for another day.
    @CommandArgument('-b', '--backend', nargs='+', choices=sorted(backends),
                     help='Which backend to build.')
    @CommandArgument('-v', '--verbose', action='store_true',
                     help='Verbose output.')
    @CommandArgument('-n', '--dry-run', action='store_true',
                     help='Do everything except writing files out.')
    def build_backend(self, backend, diff=False, verbose=False, dry_run=False):
        python = self.virtualenv_manager.python_path
        config_status = os.path.join(self.topobjdir, 'config.status')

        if not os.path.exists(config_status):
            print('config.status not found.  Please run |mach configure| '
                  'or |mach build| prior to building the %s build backend.'
                  % backend)
            return 1

        args = [python, config_status]
        if backend:
            args.append('--backend')
            args.extend(backend)
        if diff:
            args.append('--diff')
        if verbose:
            args.append('--verbose')
        if dry_run:
            args.append('--dry-run')

        return self._run_command_in_objdir(args=args, pass_thru=True,
                                           ensure_exit_code=False)
