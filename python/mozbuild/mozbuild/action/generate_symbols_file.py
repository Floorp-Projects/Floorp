# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import buildconfig
import os
from io import StringIO
from mozbuild.preprocessor import Preprocessor
from mozbuild.util import DefinesAction


def generate_symbols_file(output, *args):
    """ """
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("-D", action=DefinesAction)
    parser.add_argument("-U", action="append", default=[])
    args = parser.parse_args(args)
    input = os.path.abspath(args.input)

    pp = Preprocessor()
    pp.context.update(buildconfig.defines["ALLDEFINES"])
    if args.D:
        pp.context.update(args.D)
    for undefine in args.U:
        if undefine in pp.context:
            del pp.context[undefine]
    # Hack until MOZ_DEBUG_FLAGS are simply part of buildconfig.defines
    if buildconfig.substs.get("MOZ_DEBUG"):
        pp.context["DEBUG"] = "1"
    # Ensure @DATA@ works as expected (see the Windows section further below)
    if buildconfig.substs["OS_TARGET"] == "WINNT":
        pp.context["DATA"] = "DATA"
    else:
        pp.context["DATA"] = ""
    pp.out = StringIO()
    pp.do_filter("substitution")
    pp.do_include(input)

    symbols = [s.strip() for s in pp.out.getvalue().splitlines() if s.strip()]

    libname, ext = os.path.splitext(os.path.basename(output.name))

    if buildconfig.substs["OS_TARGET"] == "WINNT":
        # A def file is generated for MSVC link.exe that looks like the
        # following:
        # LIBRARY library.dll
        # EXPORTS
        #   symbol1
        #   symbol2
        #   ...
        #
        # link.exe however requires special markers for data symbols, so in
        # that case the symbols look like:
        #   data_symbol1 DATA
        #   data_symbol2 DATA
        #   ...
        #
        # In the input file, this is just annotated with the following syntax:
        #   data_symbol1 @DATA@
        #   data_symbol2 @DATA@
        #   ...
        # The DATA variable is "simply" expanded by the preprocessor, to
        # nothing on non-Windows, such that we only get the symbol name on
        # those platforms, and to DATA on Windows, so that the "DATA" part
        # is, in fact, part of the symbol name as far as the symbols variable
        # is concerned.
        assert ext == ".def"
        output.write("LIBRARY %s\nEXPORTS\n  %s\n" % (libname, "\n  ".join(symbols)))
    elif (
        buildconfig.substs.get("GCC_USE_GNU_LD")
        or buildconfig.substs["OS_TARGET"] == "SunOS"
    ):
        # A linker version script is generated for GNU LD that looks like the
        # following:
        # liblibrary.so {
        # global:
        #   symbol1;
        #   symbol2;
        #   ...
        # local:
        #   *;
        # };
        output.write(
            "%s {\nglobal:\n  %s;\nlocal:\n  *;\n};" % (libname, ";\n  ".join(symbols))
        )
    elif buildconfig.substs["OS_TARGET"] == "Darwin":
        # A list of symbols is generated for Apple ld that simply lists all
        # symbols, with an underscore prefix.
        output.write("".join("_%s\n" % s for s in symbols))

    return set(pp.includes)
