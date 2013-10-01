#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to generate an output header and xpt file for
# input IDL file(s). It's purpose is to directly support the build
# system. The API will change to meet the needs of the build system.

import argparse
import os
import sys

from io import BytesIO

from buildconfig import topsrcdir
from header import print_header
from typelib import write_typelib
from xpidl import IDLParser
from xpt import xpt_link

from mozbuild.makeutil import Makefile
from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.util import FileAvoidWrite


def process(input_dir, cache_dir, header_dir, xpt_dir, deps_dir, module, stems):
    p = IDLParser(outputdir=cache_dir)

    xpts = {}
    mk = Makefile()
    rule = mk.create_rule()

    # Write out dependencies for Python modules we import. If this list isn't
    # up to date, we will not re-process XPIDL files if the processor changes.
    rule.add_dependencies(iter_modules_in_path(topsrcdir))

    for stem in stems:
        path = os.path.join(input_dir, '%s.idl' % stem)
        idl_data = open(path).read()

        idl = p.parse(idl_data, filename=path)
        idl.resolve([input_dir], p)

        header_path = os.path.join(header_dir, '%s.h' % stem)
        deps_path = os.path.join(deps_dir, '%s.pp' % stem)

        xpt = BytesIO()
        write_typelib(idl, xpt, path)
        xpt.seek(0)
        xpts[stem] = xpt

        rule.add_dependencies(idl.deps)

        with FileAvoidWrite(header_path) as fh:
            print_header(idl, fh, path)

    # TODO use FileAvoidWrite once it supports binary mode.
    xpt_path = os.path.join(xpt_dir, '%s.xpt' % module)
    xpt_link(xpts.values()).write(xpt_path)

    rule.add_targets([xpt_path])
    deps_path = os.path.join(deps_dir, '%s.pp' % module)
    with FileAvoidWrite(deps_path) as fh:
        mk.dump(fh)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache-dir',
        help='Directory in which to find or write cached lexer data.')
    parser.add_argument('inputdir',
        help='Directory in which to find source .idl files.')
    parser.add_argument('headerdir',
        help='Directory in which to write header files.')
    parser.add_argument('xptdir',
        help='Directory in which to write xpt file.')
    parser.add_argument('depsdir',
        help='Directory in which to write dependency files.')
    parser.add_argument('module',
        help='Final module name to use for linked output xpt file.')
    parser.add_argument('idls', nargs='+',
        help='Source .idl file(s). Specified as stems only.')

    args = parser.parse_args(argv)
    process(args.inputdir, args.cache_dir, args.headerdir, args.xptdir,
        args.depsdir, args.module, args.idls)

if __name__ == '__main__':
    main(sys.argv[1:])
