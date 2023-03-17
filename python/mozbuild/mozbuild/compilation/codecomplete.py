# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with code completion.

from mach.decorators import Command, CommandArgument

from mozbuild.shellutil import quote as shell_quote
from mozbuild.shellutil import split as shell_split

# Instropection commands.


@Command(
    "compileflags",
    category="devenv",
    description="Display the compilation flags for a given source file",
)
@CommandArgument(
    "what", default=None, help="Source file to display compilation flags for"
)
def compileflags(command_context, what):
    from mozbuild.compilation import util
    from mozbuild.util import resolve_target_to_make

    if not util.check_top_objdir(command_context.topobjdir):
        return 1

    path_arg = command_context._wrap_path_argument(what)

    make_dir, make_target = resolve_target_to_make(
        command_context.topobjdir, path_arg.relpath()
    )

    if make_dir is None and make_target is None:
        return 1

    build_vars = util.get_build_vars(make_dir, command_context)

    if what.endswith(".c"):
        cc = "CC"
        name = "COMPILE_CFLAGS"
    else:
        cc = "CXX"
        name = "COMPILE_CXXFLAGS"

    if name not in build_vars:
        return

    # Drop the first flag since that is the pathname of the compiler.
    flags = (shell_split(build_vars[cc]) + shell_split(build_vars[name]))[1:]

    print(" ".join(shell_quote(arg) for arg in util.sanitize_cflags(flags)))
