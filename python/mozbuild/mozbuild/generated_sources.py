# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import hashlib
import json
import os

from mozpack.files import FileFinder
import mozpack.path as mozpath


def sha512_digest(data):
    '''
    Generate the SHA-512 digest of `data` and return it as a hex string.
    '''
    return hashlib.sha512(data).hexdigest()


def get_filename_with_digest(name, contents):
    '''
    Return the filename that will be used to store the generated file
    in the S3 bucket, consisting of the SHA-512 digest of `contents`
    joined with the relative path `name`.
    '''
    digest = sha512_digest(contents)
    return mozpath.join(digest, name)


def get_generated_sources():
    '''
    Yield tuples of `(objdir-rel-path, file)` for generated source files
    in this objdir, where `file` is either an absolute path to the file or
    a `mozpack.File` instance.
    '''
    import buildconfig

    # First, get the list of generated sources produced by the build backend.
    gen_sources = os.path.join(buildconfig.topobjdir, 'generated-sources.json')
    with open(gen_sources, 'r') as f:
        data = json.load(f)
    for f in data['sources']:
        yield f, mozpath.join(buildconfig.topobjdir, f)
    # Next, return all the files in $objdir/ipc/ipdl/_ipdlheaders.
    base = 'ipc/ipdl/_ipdlheaders'
    finder = FileFinder(mozpath.join(buildconfig.topobjdir, base))
    for p, f in finder.find('**/*.h'):
        yield mozpath.join(base, p), f
    # Next, return any source files that were generated into the Rust
    # object directory.
    rust_build_kind = 'debug' if buildconfig.substs.get('MOZ_DEBUG_RUST') else 'release'
    base = mozpath.join(buildconfig.substs['RUST_TARGET'],
                        rust_build_kind,
                        'build')
    finder = FileFinder(mozpath.join(buildconfig.topobjdir, base))
    for p, f in finder:
        if p.endswith(('.rs', '.c', '.h', '.cc', '.cpp')):
            yield mozpath.join(base, p), f


def get_s3_region_and_bucket():
    '''
    Return a tuple of (region, bucket) giving the AWS region and S3
    bucket to which generated sources should be uploaded.
    '''
    region = 'us-west-2'
    level = os.environ.get('MOZ_SCM_LEVEL', '1')
    bucket = {
        '1': 'gecko-generated-sources-l1',
        '2': 'gecko-generated-sources-l2',
        '3': 'gecko-generated-sources',
    }[level]
    return (region, bucket)
