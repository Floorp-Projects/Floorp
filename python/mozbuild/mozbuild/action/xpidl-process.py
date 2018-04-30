#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to generate an output header and xpt file for
# input IDL file(s). It's purpose is to directly support the build
# system. The API will change to meet the needs of the build system.

from __future__ import absolute_import

import argparse
import os
import sys

from io import BytesIO

from xpidl import jsonxpt
from buildconfig import topsrcdir
from xpidl.header import print_header
from xpidl.rust import print_rust_bindings
from xpidl.rust_macros import print_rust_macros_bindings
from xpidl.xpidl import IDLParser

from mozbuild.makeutil import Makefile
from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.util import FileAvoidWrite


def process(input_dir, inc_paths, bindings_conf, cache_dir, header_dir,
            xpcrs_dir, xpt_dir, deps_dir, module, stems):
    p = IDLParser(outputdir=cache_dir)

    xpts = []
    mk = Makefile()
    rule = mk.create_rule()

    glbl = {}
    execfile(bindings_conf, glbl)
    webidlconfig = glbl['DOMInterfaces']

    # Write out dependencies for Python modules we import. If this list isn't
    # up to date, we will not re-process XPIDL files if the processor changes.
    rule.add_dependencies(iter_modules_in_path(topsrcdir))

    for stem in stems:
        path = os.path.join(input_dir, '%s.idl' % stem)
        idl_data = open(path).read()

        idl = p.parse(idl_data, filename=path)
        idl.resolve([input_dir] + inc_paths, p, webidlconfig)

        header_path = os.path.join(header_dir, '%s.h' % stem)
        rs_rt_path = os.path.join(xpcrs_dir, 'rt', '%s.rs' % stem)
        rs_bt_path = os.path.join(xpcrs_dir, 'bt', '%s.rs' % stem)

        xpts.append(jsonxpt.build_typelib(idl))

        rule.add_dependencies(idl.deps)

        with FileAvoidWrite(header_path) as fh:
            print_header(idl, fh, path)

        with FileAvoidWrite(rs_rt_path) as fh:
            print_rust_bindings(idl, fh, path)

        with FileAvoidWrite(rs_bt_path) as fh:
            print_rust_macros_bindings(idl, fh, path)

    # NOTE: We don't use FileAvoidWrite here as we may re-run this code due to a
    # number of different changes in the code, which may not cause the .xpt
    # files to be changed in any way. This means that make will re-run us every
    # time a build is run whether or not anything changed. To fix this we
    # unconditionally write out the file.
    xpt_path = os.path.join(xpt_dir, '%s.xpt' % module)
    with open(xpt_path, 'w') as fh:
        jsonxpt.write(jsonxpt.link(xpts), fh)

    rule.add_targets([xpt_path])
    if deps_dir:
        deps_path = os.path.join(deps_dir, '%s.pp' % module)
        with FileAvoidWrite(deps_path) as fh:
            mk.dump(fh)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument('--cache-dir',
        help='Directory in which to find or write cached lexer data.')
    parser.add_argument('--depsdir',
        help='Directory in which to write dependency files.')
    parser.add_argument('--bindings-conf',
        help='Path to the WebIDL binding configuration file.')
    parser.add_argument('inputdir',
        help='Directory in which to find source .idl files.')
    parser.add_argument('headerdir',
        help='Directory in which to write header files.')
    parser.add_argument('xpcrsdir',
        help='Directory in which to write rust xpcom binding files.')
    parser.add_argument('xptdir',
        help='Directory in which to write xpt file.')
    parser.add_argument('module',
        help='Final module name to use for linked output xpt file.')
    parser.add_argument('idls', nargs='+',
        help='Source .idl file(s). Specified as stems only.')
    parser.add_argument('-I', dest='incpath', action='append', default=[],
        help='Extra directories where to look for included .idl files.')

    args = parser.parse_args(argv)
    process(args.inputdir, args.incpath, args.bindings_conf, args.cache_dir,
        args.headerdir, args.xpcrsdir, args.xptdir, args.depsdir, args.module,
        args.idls)

if __name__ == '__main__':
    main(sys.argv[1:])
