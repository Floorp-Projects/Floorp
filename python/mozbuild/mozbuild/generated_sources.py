# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

from mozpack.files import FileFinder
import mozpack.path as mozpath


def get_generated_sources():
    '''
    Yield tuples of `(objdir-rel-path, file)` for generated source files
    in this objdir, where `file` is either an absolute path to the file or
    a `mozpack.File` instance.
    '''
    import buildconfig

    # First, get the list of generated sources produced by the build backend.
    gen_sources = os.path.join(buildconfig.topobjdir, 'generated-sources.json')
    with open(gen_sources, 'rb') as f:
        data = json.load(f)
    for f in data['sources']:
        yield f, mozpath.join(buildconfig.topobjdir, f)
    # Next, return all the files in $objdir/ipc/ipdl/_ipdlheaders.
    base = 'ipc/ipdl/_ipdlheaders'
    finder = FileFinder(mozpath.join(buildconfig.topobjdir, base))
    for p, f in finder.find('**/*.h'):
        yield mozpath.join(base, p), f
    # Next, return any Rust source files that were generated into the Rust
    # object directory.
    rust_build_kind = 'debug' if buildconfig.substs.get('MOZ_DEBUG_RUST') else 'release'
    base = mozpath.join('toolkit/library',
                        buildconfig.substs['RUST_TARGET'],
                        rust_build_kind,
                        'build')
    finder = FileFinder(mozpath.join(buildconfig.topobjdir, base))
    for p, f in finder.find('**/*.rs'):
        yield mozpath.join(base, p), f
