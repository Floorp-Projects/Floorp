# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import os
import subprocess

from mach.decorators import CommandArgument, CommandProvider, Command

from mozbuild.base import MachCommandBase
from mozbuild.util import MOZBUILD_METRICS_PATH
from mozbuild.mozconfig import MozconfigLoader
import mozpack.path as mozpath

from mozbuild.backend import backends

BUILD_WHAT_HELP = """
What to build. Can be a top-level make target or a relative directory. If
multiple options are provided, they will be built serially. BUILDING ONLY PARTS
OF THE TREE CAN RESULT IN BAD TREE STATE. USE AT YOUR OWN RISK.
""".strip()


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command(
        "build",
        category="build",
        description="Build the tree.",
        metrics_path=MOZBUILD_METRICS_PATH,
    )
    @CommandArgument(
        "--jobs",
        "-j",
        default="0",
        metavar="jobs",
        type=int,
        help="Number of concurrent jobs to run. Default is the number of CPUs.",
    )
    @CommandArgument(
        "-C",
        "--directory",
        default=None,
        help="Change to a subdirectory of the build directory first.",
    )
    @CommandArgument("what", default=None, nargs="*", help=BUILD_WHAT_HELP)
    @CommandArgument(
        "-v",
        "--verbose",
        action="store_true",
        help="Verbose output for what commands the build is running.",
    )
    @CommandArgument(
        "--keep-going",
        action="store_true",
        help="Keep building after an error has occurred",
    )
    def build(
        self,
        command_context,
        what=None,
        jobs=0,
        directory=None,
        verbose=False,
        keep_going=False,
    ):
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
        from mozbuild.controller.building import BuildDriver

        command_context.log_manager.enable_all_structured_loggers()

        loader = MozconfigLoader(command_context.topsrcdir)
        mozconfig = loader.read_mozconfig(loader.AUTODETECT)
        configure_args = mozconfig["configure_args"]
        doing_pgo = configure_args and "MOZ_PGO=1" in configure_args
        # Force verbosity on automation.
        verbose = verbose or bool(os.environ.get("MOZ_AUTOMATION", False))
        append_env = None

        if doing_pgo:
            if what:
                raise Exception(
                    "Cannot specify targets (%s) in MOZ_PGO=1 builds" % what
                )
            instr = command_context._spawn(BuildDriver)
            orig_topobjdir = instr._topobjdir
            instr._topobjdir = mozpath.join(instr._topobjdir, "instrumented")

            append_env = {"MOZ_PROFILE_GENERATE": "1"}
            status = instr.build(
                command_context.metrics,
                what=what,
                jobs=jobs,
                directory=directory,
                verbose=verbose,
                keep_going=keep_going,
                mach_context=command_context._mach_context,
                append_env=append_env,
            )
            if status != 0:
                return status

            # Packaging the instrumented build is required to get the jarlog
            # data.
            status = instr._run_make(
                directory=".",
                target="package",
                silent=not verbose,
                ensure_exit_code=False,
                append_env=append_env,
            )
            if status != 0:
                return status

            pgo_env = os.environ.copy()
            if instr.config_environment.substs.get("CC_TYPE") in ("clang", "clang-cl"):
                pgo_env["LLVM_PROFDATA"] = instr.config_environment.substs.get(
                    "LLVM_PROFDATA"
                )
            pgo_env["JARLOG_FILE"] = mozpath.join(orig_topobjdir, "jarlog/en-US.log")
            pgo_cmd = [
                instr.virtualenv_manager.python_path,
                mozpath.join(command_context.topsrcdir, "build/pgo/profileserver.py"),
            ]
            subprocess.check_call(pgo_cmd, cwd=instr.topobjdir, env=pgo_env)

            # Set the default build to MOZ_PROFILE_USE
            append_env = {"MOZ_PROFILE_USE": "1"}

        driver = command_context._spawn(BuildDriver)
        return driver.build(
            command_context.metrics,
            what=what,
            jobs=jobs,
            directory=directory,
            verbose=verbose,
            keep_going=keep_going,
            mach_context=command_context._mach_context,
            append_env=append_env,
        )

    @Command(
        "configure",
        category="build",
        description="Configure the tree (run configure and config.status).",
        metrics_path=MOZBUILD_METRICS_PATH,
    )
    @CommandArgument(
        "options", default=None, nargs=argparse.REMAINDER, help="Configure options"
    )
    def configure(
        self,
        command_context,
        options=None,
        buildstatus_messages=False,
        line_handler=None,
    ):
        from mozbuild.controller.building import BuildDriver

        command_context.log_manager.enable_all_structured_loggers()
        driver = command_context._spawn(BuildDriver)

        return driver.configure(
            command_context.metrics,
            options=options,
            buildstatus_messages=buildstatus_messages,
            line_handler=line_handler,
        )

    @Command(
        "resource-usage",
        category="post-build",
        description="Show information about system resource usage for a build.",
    )
    @CommandArgument(
        "--address",
        default="localhost",
        help="Address the HTTP server should listen on.",
    )
    @CommandArgument(
        "--port",
        type=int,
        default=0,
        help="Port number the HTTP server should listen on.",
    )
    @CommandArgument(
        "--browser",
        default="firefox",
        help="Web browser to automatically open. See webbrowser Python module.",
    )
    @CommandArgument("--url", help="URL of JSON document to display")
    def resource_usage(
        self, command_context, address=None, port=None, browser=None, url=None
    ):
        import webbrowser
        from mozbuild.html_build_viewer import BuildViewerServer

        server = BuildViewerServer(address, port)

        if url:
            server.add_resource_json_url("url", url)
        else:
            last = command_context._get_state_filename("build_resources.json")
            if not os.path.exists(last):
                print(
                    "Build resources not available. If you have performed a "
                    "build and receive this message, the psutil Python package "
                    "likely failed to initialize properly."
                )
                return 1

            server.add_resource_json_file("last", last)
        try:
            webbrowser.get(browser).open_new_tab(server.url)
        except Exception:
            print("Cannot get browser specified, trying the default instead.")
            try:
                browser = webbrowser.get().open_new_tab(server.url)
            except Exception:
                print("Please open %s in a browser." % server.url)

        print("Hit CTRL+c to stop server.")
        server.run()

    @Command(
        "build-backend",
        category="build",
        description="Generate a backend used to build the tree.",
    )
    @CommandArgument(
        "-d", "--diff", action="store_true", help="Show a diff of changes."
    )
    # It would be nice to filter the choices below based on
    # conditions, but that is for another day.
    @CommandArgument(
        "-b",
        "--backend",
        nargs="+",
        choices=sorted(backends),
        help="Which backend to build.",
    )
    @CommandArgument("-v", "--verbose", action="store_true", help="Verbose output.")
    @CommandArgument(
        "-n",
        "--dry-run",
        action="store_true",
        help="Do everything except writing files out.",
    )
    def build_backend(
        self, command_context, backend, diff=False, verbose=False, dry_run=False
    ):
        python = command_context.virtualenv_manager.python_path
        config_status = os.path.join(command_context.topobjdir, "config.status")

        if not os.path.exists(config_status):
            print(
                "config.status not found.  Please run |mach configure| "
                "or |mach build| prior to building the %s build backend." % backend
            )
            return 1

        args = [python, config_status]
        if backend:
            args.append("--backend")
            args.extend(backend)
        if diff:
            args.append("--diff")
        if verbose:
            args.append("--verbose")
        if dry_run:
            args.append("--dry-run")

        return command_context._run_command_in_objdir(
            args=args, pass_thru=True, ensure_exit_code=False
        )
