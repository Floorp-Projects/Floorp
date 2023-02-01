# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import logging
import subprocess
import sys
from datetime import datetime, timedelta
from operator import itemgetter

from mach.decorators import Command, CommandArgument, SubCommand
from mozbuild.base import MozbuildObject


def _get_busted_bugs(payload):
    import requests

    payload = dict(payload)
    payload["include_fields"] = "id,summary,last_change_time,resolution"
    payload["blocks"] = 1543241
    response = requests.get("https://bugzilla.mozilla.org/rest/bug", payload)
    response.raise_for_status()
    return response.json().get("bugs", [])


@Command(
    "busted",
    category="misc",
    description="Query known bugs in our tooling, and file new ones.",
)
def busted_default(command_context):
    unresolved = _get_busted_bugs({"resolution": "---"})
    creation_time = datetime.now() - timedelta(days=15)
    creation_time = creation_time.strftime("%Y-%m-%dT%H-%M-%SZ")
    resolved = _get_busted_bugs({"creation_time": creation_time})
    resolved = [bug for bug in resolved if bug["resolution"]]
    all_bugs = sorted(
        unresolved + resolved, key=itemgetter("last_change_time"), reverse=True
    )
    if all_bugs:
        for bug in all_bugs:
            print(
                "[%s] Bug %s - %s"
                % (
                    "UNRESOLVED"
                    if not bug["resolution"]
                    else "RESOLVED - %s" % bug["resolution"],
                    bug["id"],
                    bug["summary"],
                )
            )
    else:
        print("No known tooling issues found.")


@SubCommand("busted", "file", description="File a bug for busted tooling.")
@CommandArgument(
    "against",
    help=(
        "The specific mach command that is busted (i.e. if you encountered "
        "an error with `mach build`, run `mach busted file build`). If "
        "the issue is not connected to any particular mach command, you "
        "can also run `mach busted file general`."
    ),
)
def busted_file(command_context, against):
    import webbrowser

    if (
        against != "general"
        and against not in command_context._mach_context.commands.command_handlers
    ):
        print(
            "%s is not a valid value for `against`. `against` must be "
            "the name of a `mach` command, or else the string "
            '"general".' % against
        )
        return 1

    if against == "general":
        product = "Firefox Build System"
        component = "General"
    else:
        import inspect

        import mozpack.path as mozpath

        # Look up the file implementing that command, then cross-refernce
        # moz.build files to get the product/component.
        handler = command_context._mach_context.commands.command_handlers[against]
        sourcefile = mozpath.relpath(
            inspect.getsourcefile(handler.func), command_context.topsrcdir
        )
        reader = command_context.mozbuild_reader(config_mode="empty")
        try:
            res = reader.files_info([sourcefile])[sourcefile]["BUG_COMPONENT"]
            product, component = res.product, res.component
        except TypeError:
            # The file might not have a bug set.
            product = "Firefox Build System"
            component = "General"

    uri = (
        "https://bugzilla.mozilla.org/enter_bug.cgi?"
        "product=%s&component=%s&blocked=1543241" % (product, component)
    )
    webbrowser.open_new_tab(uri)


MACH_PASTEBIN_DURATIONS = {
    "onetime": "onetime",
    "hour": "3600",
    "day": "86400",
    "week": "604800",
    "month": "2073600",
}

EXTENSION_TO_HIGHLIGHTER = {
    ".hgrc": "ini",
    "Dockerfile": "docker",
    "Makefile": "make",
    "applescript": "applescript",
    "arduino": "arduino",
    "bash": "bash",
    "bat": "bat",
    "c": "c",
    "clojure": "clojure",
    "cmake": "cmake",
    "coffee": "coffee-script",
    "console": "console",
    "cpp": "cpp",
    "cs": "csharp",
    "css": "css",
    "cu": "cuda",
    "cuda": "cuda",
    "dart": "dart",
    "delphi": "delphi",
    "diff": "diff",
    "django": "django",
    "docker": "docker",
    "elixir": "elixir",
    "erlang": "erlang",
    "go": "go",
    "h": "c",
    "handlebars": "handlebars",
    "haskell": "haskell",
    "hs": "haskell",
    "html": "html",
    "ini": "ini",
    "ipy": "ipythonconsole",
    "ipynb": "ipythonconsole",
    "irc": "irc",
    "j2": "django",
    "java": "java",
    "js": "js",
    "json": "json",
    "jsx": "jsx",
    "kt": "kotlin",
    "less": "less",
    "lisp": "common-lisp",
    "lsp": "common-lisp",
    "lua": "lua",
    "m": "objective-c",
    "make": "make",
    "matlab": "matlab",
    "md": "_markdown",
    "nginx": "nginx",
    "numpy": "numpy",
    "patch": "diff",
    "perl": "perl",
    "php": "php",
    "pm": "perl",
    "postgresql": "postgresql",
    "py": "python",
    "rb": "rb",
    "rs": "rust",
    "rst": "rst",
    "sass": "sass",
    "scss": "scss",
    "sh": "bash",
    "sol": "sol",
    "sql": "sql",
    "swift": "swift",
    "tex": "tex",
    "typoscript": "typoscript",
    "vim": "vim",
    "xml": "xml",
    "xslt": "xslt",
    "yaml": "yaml",
    "yml": "yaml",
}


def guess_highlighter_from_path(path):
    """Return a known highlighter from a given path

    Attempt to select a highlighter by checking the file extension in the mapping
    of extensions to highlighter. If that fails, attempt to pass the basename of
    the file. Return `_code` as the default highlighter if that fails.
    """
    import os

    _name, ext = os.path.splitext(path)

    if ext.startswith("."):
        ext = ext[1:]

    if ext in EXTENSION_TO_HIGHLIGHTER:
        return EXTENSION_TO_HIGHLIGHTER[ext]

    basename = os.path.basename(path)

    return EXTENSION_TO_HIGHLIGHTER.get(basename, "_code")


PASTEMO_MAX_CONTENT_LENGTH = 250 * 1024 * 1024

PASTEMO_URL = "https://paste.mozilla.org/api/"


@Command(
    "pastebin",
    category="misc",
    description="Command line interface to paste.mozilla.org.",
)
@CommandArgument(
    "--list-highlighters",
    action="store_true",
    help="List known highlighters and exit",
)
@CommandArgument(
    "--highlighter", default=None, help="Syntax highlighting to use for paste"
)
@CommandArgument(
    "--expires",
    default="week",
    choices=sorted(MACH_PASTEBIN_DURATIONS.keys()),
    help="Expire paste after given time duration (default: %(default)s)",
)
@CommandArgument(
    "--verbose",
    action="store_true",
    help="Print extra info such as selected syntax highlighter",
)
@CommandArgument(
    "path",
    nargs="?",
    default=None,
    help="Path to file for upload to paste.mozilla.org",
)
def pastebin(command_context, list_highlighters, highlighter, expires, verbose, path):
    """Command line interface to `paste.mozilla.org`.

    Takes either a filename whose content should be pasted, or reads
    content from standard input. If a highlighter is specified it will
    be used, otherwise the file name will be used to determine an
    appropriate highlighter.
    """

    import requests

    def verbose_print(*args, **kwargs):
        """Print a string if `--verbose` flag is set"""
        if verbose:
            print(*args, **kwargs)

    # Show known highlighters and exit.
    if list_highlighters:
        lexers = set(EXTENSION_TO_HIGHLIGHTER.values())
        print("Available lexers:\n    - %s" % "\n    - ".join(sorted(lexers)))
        return 0

    # Get a correct expiry value.
    try:
        verbose_print("Setting expiry from %s" % expires)
        expires = MACH_PASTEBIN_DURATIONS[expires]
        verbose_print("Using %s as expiry" % expires)
    except KeyError:
        print(
            "%s is not a valid duration.\n"
            "(hint: try one of %s)"
            % (expires, ", ".join(MACH_PASTEBIN_DURATIONS.keys()))
        )
        return 1

    data = {
        "format": "json",
        "expires": expires,
    }

    # Get content to be pasted.
    if path:
        verbose_print("Reading content from %s" % path)
        try:
            with open(path, "r") as f:
                content = f.read()
        except IOError:
            print("ERROR. No such file %s" % path)
            return 1

        lexer = guess_highlighter_from_path(path)
        if lexer:
            data["lexer"] = lexer
    else:
        verbose_print("Reading content from stdin")
        content = sys.stdin.read()

    # Assert the length of content to be posted does not exceed the maximum.
    content_length = len(content)
    verbose_print("Checking size of content is okay (%d)" % content_length)
    if content_length > PASTEMO_MAX_CONTENT_LENGTH:
        print(
            "Paste content is too large (%d, maximum %d)"
            % (content_length, PASTEMO_MAX_CONTENT_LENGTH)
        )
        return 1

    data["content"] = content

    # Highlight as specified language, overwriting value set from filename.
    if highlighter:
        verbose_print("Setting %s as highlighter" % highlighter)
        data["lexer"] = highlighter

    try:
        verbose_print("Sending request to %s" % PASTEMO_URL)
        resp = requests.post(PASTEMO_URL, data=data)

        # Error code should always be 400.
        # Response content will include a helpful error message,
        # so print it here (for example, if an invalid highlighter is
        # provided, it will return a list of valid highlighters).
        if resp.status_code >= 400:
            print("Error code %d: %s" % (resp.status_code, resp.content))
            return 1

        verbose_print("Pasted successfully")

        response_json = resp.json()

        verbose_print("Paste highlighted as %s" % response_json["lexer"])
        print(response_json["url"])

        return 0
    except Exception as e:
        print("ERROR. Paste failed.")
        print("%s" % e)
    return 1


class PypiBasedTool:
    """
    Helper for loading a tool that is hosted on pypi. The package is expected
    to expose a `mach_interface` module which has `new_release_on_pypi`,
    `parser`, and `run` functions.
    """

    def __init__(self, module_name, pypi_name=None):
        self.name = module_name
        self.pypi_name = pypi_name or module_name

    def _import(self):
        # Lazy loading of the tools mach interface.
        # Note that only the mach_interface module should be used from this file.
        import importlib

        try:
            return importlib.import_module("%s.mach_interface" % self.name)
        except ImportError:
            return None

    def create_parser(self, subcommand=None):
        # Create the command line parser.
        # If the tool is not installed, or not up to date, it will
        # first be installed.
        cmd = MozbuildObject.from_environment()
        cmd.activate_virtualenv()
        tool = self._import()
        if not tool:
            # The tool is not here at all, install it
            cmd.virtualenv_manager.install_pip_package(self.pypi_name)
            print(
                "%s was installed. please re-run your"
                " command. If you keep getting this message please "
                " manually run: 'pip install -U %s'." % (self.pypi_name, self.pypi_name)
            )
        else:
            # Check if there is a new release available
            release = tool.new_release_on_pypi()
            if release:
                print(release)
                # there is one, so install it. Note that install_pip_package
                # does not work here, so just run pip directly.
                subprocess.check_call(
                    [
                        cmd.virtualenv_manager.python_path,
                        "-m",
                        "pip",
                        "install",
                        f"{self.pypi_name}=={release}",
                    ]
                )
                print(
                    "%s was updated to version %s. please"
                    " re-run your command." % (self.pypi_name, release)
                )
            else:
                # Tool is up to date, return the parser.
                if subcommand:
                    return tool.parser(subcommand)
                else:
                    return tool.parser()
        # exit if we updated or installed mozregression because
        # we may have already imported mozregression and running it
        # as this may cause issues.
        sys.exit(0)

    def run(self, **options):
        tool = self._import()
        tool.run(options)


def mozregression_create_parser():
    # Create the mozregression command line parser.
    # if mozregression is not installed, or not up to date, it will
    # first be installed.
    loader = PypiBasedTool("mozregression")
    return loader.create_parser()


@Command(
    "mozregression",
    category="misc",
    description="Regression range finder for nightly and inbound builds.",
    parser=mozregression_create_parser,
)
def run(command_context, **options):
    command_context.activate_virtualenv()
    mozregression = PypiBasedTool("mozregression")
    mozregression.run(**options)


@Command(
    "node",
    category="devenv",
    description="Run the NodeJS interpreter used for building.",
)
@CommandArgument("args", nargs=argparse.REMAINDER)
def node(command_context, args):
    from mozbuild.nodeutil import find_node_executable

    # Avoid logging the command
    command_context.log_manager.terminal_handler.setLevel(logging.CRITICAL)

    node_path, _ = find_node_executable()

    return command_context.run_process(
        [node_path] + args,
        pass_thru=True,  # Allow user to run Node interactively.
        ensure_exit_code=False,  # Don't throw on non-zero exit code.
    )


@Command(
    "npm",
    category="devenv",
    description="Run the npm executable from the NodeJS used for building.",
)
@CommandArgument("args", nargs=argparse.REMAINDER)
def npm(command_context, args):
    from mozbuild.nodeutil import find_npm_executable

    # Avoid logging the command
    command_context.log_manager.terminal_handler.setLevel(logging.CRITICAL)

    import os

    # Add node and npm from mozbuild to front of system path
    #
    # This isn't pretty, but npm currently executes itself with
    # `#!/usr/bin/env node`, which means it just uses the node in the
    # current PATH. As a result, stuff gets built wrong and installed
    # in the wrong places and probably other badness too without this:
    npm_path, _ = find_npm_executable()
    if not npm_path:
        exit(-1, "could not find npm executable")
    path = os.path.abspath(os.path.dirname(npm_path))
    os.environ["PATH"] = "{}{}{}".format(path, os.pathsep, os.environ["PATH"])

    # karma-firefox-launcher needs the path to firefox binary.
    firefox_bin = command_context.get_binary_path(validate_exists=False)
    if os.path.exists(firefox_bin):
        os.environ["FIREFOX_BIN"] = firefox_bin

    return command_context.run_process(
        [npm_path, "--scripts-prepend-node-path=auto"] + args,
        pass_thru=True,  # Avoid eating npm output/error messages
        ensure_exit_code=False,  # Don't throw on non-zero exit code.
    )


def logspam_create_parser(subcommand):
    # Create the logspam command line parser.
    # if logspam is not installed, or not up to date, it will
    # first be installed.
    loader = PypiBasedTool("logspam", "mozilla-log-spam")
    return loader.create_parser(subcommand)


from functools import partial


@Command(
    "logspam",
    category="misc",
    description="Warning categorizer for treeherder test runs.",
)
def logspam(command_context):
    pass


@SubCommand("logspam", "report", parser=partial(logspam_create_parser, "report"))
def report(command_context, **options):
    command_context.activate_virtualenv()
    logspam = PypiBasedTool("logspam")
    logspam.run(command="report", **options)


@SubCommand("logspam", "bisect", parser=partial(logspam_create_parser, "bisect"))
def bisect(command_context, **options):
    command_context.activate_virtualenv()
    logspam = PypiBasedTool("logspam")
    logspam.run(command="bisect", **options)


@SubCommand("logspam", "file", parser=partial(logspam_create_parser, "file"))
def create(command_context, **options):
    command_context.activate_virtualenv()
    logspam = PypiBasedTool("logspam")
    logspam.run(command="file", **options)


# mots_loader will be used when running commands and subcommands, as well as
# when creating the parsers.
mots_loader = PypiBasedTool("mots")


def mots_create_parser(subcommand=None):
    return mots_loader.create_parser(subcommand)


def mots_run_subcommand(command, command_context, **options):
    command_context.activate_virtualenv()
    mots_loader.run(command=command, **options)


class motsSubCommand(SubCommand):
    """A helper subclass that reduces repitition when defining subcommands."""

    def __init__(self, subcommand):
        super().__init__(
            "mots",
            subcommand,
            parser=partial(mots_create_parser, subcommand),
        )


@Command(
    "mots",
    category="misc",
    description="Manage module information in-tree using the mots CLI.",
    parser=mots_create_parser,
)
def mots(command_context, **options):
    """The main mots command call."""
    command_context.activate_virtualenv()
    mots_loader.run(**options)


# Define subcommands that will be proxied through mach.
for sc in (
    "clean",
    "check-hashes",
    "export",
    "export-and-clean",
    "module",
    "query",
    "settings",
    "user",
    "validate",
):
    # Pass through args and kwargs, but add the subcommand string as the first argument.
    motsSubCommand(sc)(lambda *a, **kw: mots_run_subcommand(sc, *a, **kw))
