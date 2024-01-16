# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import subprocess
import sys
import traceback
from pathlib import Path
from textwrap import dedent

import six
from mozboot.mozconfig import find_mozconfig
from mozpack import path as mozpath

MOZCONFIG_BAD_EXIT_CODE = """
Evaluation of your mozconfig exited with an error. This could be triggered
by a command inside your mozconfig failing. Please change your mozconfig
to not error and/or to catch errors in executed commands.
""".strip()

MOZCONFIG_BAD_OUTPUT = """
Evaluation of your mozconfig produced unexpected output.  This could be
triggered by a command inside your mozconfig failing or producing some warnings
or error messages. Please change your mozconfig to not error and/or to catch
errors in executed commands.
""".strip()


class MozconfigLoadException(Exception):
    """Raised when a mozconfig could not be loaded properly.

    This typically indicates a malformed or misbehaving mozconfig file.
    """

    def __init__(self, path, message, output=None):
        self.path = path
        self.output = output

        message = (
            dedent(
                """
        Error loading mozconfig: {path}

        {message}
        """
            )
            .format(path=self.path, message=message)
            .lstrip()
        )

        if self.output:
            message += dedent(
                """
            mozconfig output:

            {output}
            """
            ).format(output="\n".join([six.ensure_text(s) for s in self.output]))

        Exception.__init__(self, message)


class MozconfigLoader(object):
    """Handles loading and parsing of mozconfig files."""

    RE_MAKE_VARIABLE = re.compile(
        r"""
        ^\s*                    # Leading whitespace
        (?P<var>[a-zA-Z_0-9]+)  # Variable name
        \s* [?:]?= \s*          # Assignment operator surrounded by optional
                                # spaces
        (?P<value>.*$)""",  # Everything else (likely the value)
        re.VERBOSE,
    )

    IGNORE_SHELL_VARIABLES = {"_", "BASH_ARGV", "BASH_ARGV0", "BASH_ARGC"}

    ENVIRONMENT_VARIABLES = {"CC", "CXX", "CFLAGS", "CXXFLAGS", "LDFLAGS", "MOZ_OBJDIR"}

    AUTODETECT = object()

    def __init__(self, topsrcdir):
        self.topsrcdir = topsrcdir

    @property
    def _loader_script(self):
        our_dir = os.path.abspath(os.path.dirname(__file__))

        return os.path.join(our_dir, "mozconfig_loader")

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
            path = find_mozconfig(self.topsrcdir)
        if isinstance(path, Path):
            path = str(path)

        result = {
            "path": path,
            "topobjdir": None,
            "configure_args": None,
            "make_flags": None,
            "make_extra": None,
            "env": None,
            "vars": None,
        }

        if path is None:
            if "MOZ_OBJDIR" in os.environ:
                result["topobjdir"] = os.environ["MOZ_OBJDIR"]
            return result

        path = mozpath.normsep(path)

        result["configure_args"] = []
        result["make_extra"] = []
        result["make_flags"] = []

        # Since mozconfig_loader is a shell script, running it "normally"
        # actually leads to two shell executions on Windows. Avoid this by
        # directly calling sh mozconfig_loader.
        shell = "sh"
        env = dict(os.environ)
        env["PYTHONIOENCODING"] = "utf-8"

        if "MOZILLABUILD" in os.environ:
            mozillabuild = os.environ["MOZILLABUILD"]
            if (Path(mozillabuild) / "msys2").exists():
                shell = mozillabuild + "/msys2/usr/bin/sh"
            else:
                shell = mozillabuild + "/msys/bin/sh"
            prefer_mozillabuild_path = [
                os.path.dirname(shell),
                str(Path(mozillabuild) / "bin"),
                env["PATH"],
            ]
            env["PATH"] = os.pathsep.join(prefer_mozillabuild_path)
        if sys.platform == "win32":
            shell = shell + ".exe"

        command = [
            mozpath.normsep(shell),
            mozpath.normsep(self._loader_script),
            mozpath.normsep(self.topsrcdir),
            mozpath.normsep(path),
            mozpath.normsep(sys.executable),
            mozpath.join(mozpath.dirname(self._loader_script), "action", "dump_env.py"),
        ]

        try:
            # We need to capture stderr because that's where the shell sends
            # errors if execution fails.
            output = six.ensure_text(
                subprocess.check_output(
                    command,
                    stderr=subprocess.STDOUT,
                    cwd=self.topsrcdir,
                    env=env,
                    universal_newlines=True,
                    encoding="utf-8",
                )
            )
        except subprocess.CalledProcessError as e:
            lines = e.output.splitlines()

            # Output before actual execution shouldn't be relevant.
            try:
                index = lines.index("------END_BEFORE_SOURCE")
                lines = lines[index + 1 :]
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
            print("Assertion failed in _parse_loader_output:")
            traceback.print_exc()
            raise MozconfigLoadException(
                path, MOZCONFIG_BAD_OUTPUT, output.splitlines()
            )

        def diff_vars(vars_before, vars_after):
            set1 = set(vars_before.keys()) - self.IGNORE_SHELL_VARIABLES
            set2 = set(vars_after.keys()) - self.IGNORE_SHELL_VARIABLES
            added = set2 - set1
            removed = set1 - set2
            maybe_modified = set1 & set2
            changed = {"added": {}, "removed": {}, "modified": {}, "unmodified": {}}

            for key in added:
                changed["added"][key] = vars_after[key]

            for key in removed:
                changed["removed"][key] = vars_before[key]

            for key in maybe_modified:
                if vars_before[key] != vars_after[key]:
                    changed["modified"][key] = (vars_before[key], vars_after[key])
                elif key in self.ENVIRONMENT_VARIABLES:
                    # In order for irrelevant environment variable changes not
                    # to incur in re-running configure, only a set of
                    # environment variables are stored when they are
                    # unmodified. Otherwise, changes such as using a different
                    # terminal window, or even rebooting, would trigger
                    # reconfigures.
                    changed["unmodified"][key] = vars_after[key]

            return changed

        result["env"] = diff_vars(parsed["env_before"], parsed["env_after"])

        # Environment variables also appear as shell variables, but that's
        # uninteresting duplication of information. Filter them out.
        def filt(x, y):
            return {k: v for k, v in x.items() if k not in y}

        result["vars"] = diff_vars(
            filt(parsed["vars_before"], parsed["env_before"]),
            filt(parsed["vars_after"], parsed["env_after"]),
        )

        result["configure_args"] = [self._expand(o) for o in parsed["ac"]]

        if "MOZ_OBJDIR" in parsed["env_before"]:
            result["topobjdir"] = parsed["env_before"]["MOZ_OBJDIR"]

        mk = [self._expand(o) for o in parsed["mk"]]

        for o in mk:
            match = self.RE_MAKE_VARIABLE.match(o)

            if match is None:
                result["make_extra"].append(o)
                continue

            name, value = match.group("var"), match.group("value")

            if name == "MOZ_MAKE_FLAGS":
                result["make_flags"] = value.split()
                continue

            if name == "MOZ_OBJDIR":
                result["topobjdir"] = value
                if parsed["env_before"].get("MOZ_PROFILE_GENERATE") == "1":
                    # If MOZ_OBJDIR is specified in the mozconfig, we need to
                    # make sure that the '/instrumented' directory gets appended
                    # for the first build to avoid an objdir mismatch when
                    # running 'mach package' on Windows.
                    result["topobjdir"] = mozpath.join(
                        result["topobjdir"], "instrumented"
                    )
                continue

            result["make_extra"].append(o)

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
            if not line:
                continue

            if line.startswith("------BEGIN_"):
                assert current_type is None
                assert current is None
                assert not in_variable
                current_type = line[len("------BEGIN_") :]
                current = []
                continue

            if line.startswith("------END_"):
                assert not in_variable
                section = line[len("------END_") :]
                assert current_type == section

                if current_type == "AC_OPTION":
                    ac_options.append("\n".join(current))
                elif current_type == "MK_OPTION":
                    mk_options.append("\n".join(current))

                current = None
                current_type = None
                continue

            assert current_type is not None

            vars_mapping = {
                "BEFORE_SOURCE": before_source,
                "AFTER_SOURCE": after_source,
                "ENV_BEFORE_SOURCE": env_before_source,
                "ENV_AFTER_SOURCE": env_after_source,
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
                        value = "\n".join(current)
                        in_variable = None
                    else:
                        current.append(line)
                        continue
                else:
                    equal_pos = line.find("=")

                    if equal_pos < 1:
                        # TODO log warning?
                        continue

                    name = line[0:equal_pos]
                    value = line[equal_pos + 1 :]

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
            "mk": mk_options,
            "ac": ac_options,
            "vars_before": before_source,
            "vars_after": after_source,
            "env_before": env_before_source,
            "env_after": env_after_source,
        }

    def _expand(self, s):
        return s.replace("@TOPSRCDIR@", self.topsrcdir)
